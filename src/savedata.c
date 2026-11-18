/* Copyright (C) 2023 Nunuhara Cabbage <nunuhara@haniwa.technology>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <stdio.h>
#include <stdbool.h>

#include "nulib.h"
#include "nulib/file.h"
#include "nulib/little_endian.h"

#include "game.h"
#include "memory.h"
#include "savedata.h"
#include "vm.h"

static void close_save(FILE *f)
{
	if (fclose(f))
		WARNING("fclose: %s", strerror(errno));
}

/*
 * Save folder (launcher): if AI5_SAVE_DIR is set (e.g. "launcher-save"), every
 * save file lives in that folder inside the game folder, so the original PC
 * saves are never mixed with or overwritten by ours.  Returns NULL when unset.
 */
const char *savedata_dir(void)
{
	static bool init = false;
	static char *dir = NULL;
	if (init)
		return dir;
	init = true;
	const char *env = getenv("AI5_SAVE_DIR");
	if (!env || !*env)
		return NULL;
	dir = path_get_icase(env);
	if (!dir) {
		if (mkdir_p(env))
			WARNING("cannot create save folder \"%s\": %s", env, strerror(errno));
		dir = strdup(env);
	}
	NOTICE("save folder: %s", dir);
	return dir;
}

// true if the name already points into the save folder (e.g. hanahebi.c)
static bool savedata_in_dir(const char *save_name)
{
	const char *dir = savedata_dir();
	size_t len = dir ? strlen(dir) : 0;
	return dir && !strncasecmp(save_name, dir, len) && save_name[len] == '/';
}

/*
 * Path of a save file: "<save folder>/<name>" (matching an existing file
 * case-insensitively), or just <name> when no save folder is set.
 */
char *savedata_path(const char *save_name)
{
	const char *dir = savedata_dir();
	if (!dir || savedata_in_dir(save_name))
		return strdup(save_name);
	char *joined = xmalloc(strlen(dir) + strlen(save_name) + 2);
	sprintf(joined, "%s/%s", dir, save_name);
	char *found = path_get_icase(joined);
	if (found) {
		free(joined);
		return found;
	}
	return joined;
}

static bool copy_file(const char *src, const char *dst)
{
	FILE *in = file_open_utf8(src, "rb");
	if (!in)
		return false;
	FILE *out = file_open_utf8(dst, "wb");
	if (!out) {
		fclose(in);
		return false;
	}
	uint8_t buf[16384];
	size_t n;
	bool ok = true;
	while ((n = fread(buf, 1, sizeof(buf), in)) > 0) {
		if (fwrite(buf, 1, n, out) != n) {
			ok = false;
			break;
		}
	}
	fclose(in);
	if (fclose(out))
		ok = false;
	return ok;
}

// XXX: Save files should be shipped with the game, but if not we create them.
static void create_save(const char *save_name)
{
	FILE *f = file_open_utf8(save_name, "wb");
	if (!f) {
		WARNING("fopen: %s", strerror(errno));
		return;
	}
	uint8_t buf[MEMORY_MEM16_MAX_SIZE];
	memset(buf, 0, game->mem16_size);
	if (fwrite(buf, game->mem16_size, 1, f) != 1)
		WARNING("fwrite: %s", strerror(errno));
	close_save(f);
}

static FILE *open_save(const char *save_name, const char *mode)
{
	if (savedata_dir()) {
		char *path = savedata_path(save_name);
		FILE *f = file_open_utf8(path, mode);
		if (!f) {
			// first use: start from the file shipped with the game
			// (read only — the original is never written), else empty
			char *orig = savedata_in_dir(save_name) ? NULL
				: path_get_icase(save_name);
			if (!orig || !copy_file(orig, path))
				create_save(path);
			free(orig);
			f = file_open_utf8(path, mode);
		}
		free(path);
		return f;
	}
	char *path = path_get_icase(save_name);
	if (!path) {
		create_save(save_name);
		path = strdup(save_name);
		WARNING("Save file \"%s\" doesn't exist", save_name);
	}
	FILE *f = file_open_utf8(path, mode);
	free(path);
	return f;
}

void savedata_read(const char *save_name, uint8_t *buf, uint32_t off, size_t size)
{
	FILE *f = open_save(save_name, "rb");
	if (!f) {
		WARNING("Failed to open save file \"%s\": %s", save_name, strerror(errno));
		return;
	}
	if (off && fseek(f, off, SEEK_SET) < 0)
		WARNING("fseek: %s", strerror(errno));
	if (fread(buf + off, size, 1, f) != 1)
		WARNING("fread: %s", strerror(errno));
	close_save(f);
}

void savedata_write(const char *save_name, const uint8_t *buf, uint32_t off, size_t size)
{
	// XXX: we open r+b (O_RDWR) because it's the only write mode that doesn't
	//      truncate or append
	FILE *f = open_save(save_name, "r+b");
	if (!f) {
		WARNING("Failed to open save file \"%s\": %s", save_name, strerror(errno));
		return;
	}
	if (off && fseek(f, off, SEEK_SET) < 0)
		WARNING("fseek: %s", strerror(errno));
	if (fwrite(buf + off, size, 1, f) != 1)
		WARNING("fwrite: %s", strerror(errno));
	close_save(f);
}

void savedata_resume_load(const char *save_name)
{
	savedata_read(save_name, memory_raw, 0, game->mem16_size);
	game->mem_restore();
	vm_load_mes(mem_mes_name());
	vm_flag_on(FLAG_RETURN);
}

void savedata_resume_save(const char *save_name)
{
	savedata_write(save_name, memory_raw, 0, game->mem16_size);
}

void savedata_load(const char *save_name, unsigned start)
{
	// load except mes name
	savedata_read(save_name, memory_raw, start, game->mem16_size - start);
	game->mem_restore();
}

void savedata_save(const char *save_name, unsigned start)
{
	// save except mes name
	savedata_write(save_name, memory_raw, start, game->mem16_size - start);
}

void savedata_load_var4(const char *save_name)
{
	savedata_read(save_name, memory_raw, memory_ptr.var4 - memory_raw, game->var4_size);
}

void savedata_load_var4_restore(const char *save_name)
{
	savedata_read(save_name, memory_raw, memory_ptr.var4 - memory_raw, game->var4_size);
	game->mem_restore();
}

void savedata_save_var4(const char *save_name)
{
	savedata_write(save_name, memory_raw, memory_ptr.var4 - memory_raw, game->var4_size);
}

void savedata_save_union_var4(const char *save_name)
{
	uint8_t buf[MEMORY_VAR4_MAX_SIZE];
	const size_t var4_off = memory_ptr.var4 - memory_raw;
	FILE *f = open_save(save_name, "r+b");
	if (!f) {
		WARNING("Failed to open save file \"%s\": %s", save_name, strerror(errno));
		return;
	}
	if (fseek(f, var4_off, SEEK_SET) < 0) {
		WARNING("fseek: %s", strerror(errno));
		goto end;
	}
	if (fread(buf, game->var4_size, 1, f) != 1) {
		WARNING("fread: %s", strerror(errno));
		goto end;
	}

	uint8_t *var4 = memory_ptr.var4;
	for (unsigned i = 0; i < game->var4_size; i++) {
		buf[i] |= var4[i];
	}

	if (fseek(f, var4_off, SEEK_SET) < 0) {
		WARNING("fseek: %s", strerror(errno));
		goto end;
	}
	if (fwrite(buf, game->var4_size, 1, f) != 1)
		WARNING("fwrite: %s", strerror(errno));
end:
	close_save(f);
}

void savedata_load_var4_slice(const char *save_name, unsigned from, unsigned to)
{
	if (from > to)
		return;
	savedata_read(save_name, memory_raw, (memory_ptr.var4 - memory_raw) + from,
			(to + 1) - from);
}

void savedata_save_var4_slice(const char *save_name, unsigned from, unsigned to)
{
	if (from > to)
		return;
	savedata_write(save_name, memory_raw, (memory_ptr.var4 - memory_raw) + from,
			(to + 1) - from);
}

void savedata_copy(const char *src_save, const char *dst_save)
{
	uint8_t buf[MEMORY_MEM16_MAX_SIZE];
	savedata_read(src_save, buf, 0, game->mem16_size);
	savedata_write(dst_save, buf, 0, game->mem16_size);
}

void savedata_set_mes_name(const char *save_name, const char *mes_name)
{
	savedata_write(save_name, (const uint8_t*)mes_name, 0, strlen(mes_name) + 1);
}

void savedata_load_variables(const char *save_name, const char *vars)
{
	unsigned var16_off = memory_ptr.var16 - memory_raw;
	uint8_t save[MEMORY_MEM16_MAX_SIZE];
	savedata_read(save_name, save, var16_off, 26 * 2);

	for (const char *str = vars; *str; str++) {
		if (*str < 'A' || *str > 'Z') {
			WARNING("Invalid variable name: %c", *str);
			return;
		}
		unsigned varno = *str - 'A';
		mem_set_var16(varno, le_get16(save, var16_off + varno * 2));
	}
}
