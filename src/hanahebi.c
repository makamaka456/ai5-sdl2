/* Copyright (C) 2026 ai5-sdl2 Korean port contributors
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

/*
 * 花と蛇 (Hana to Hebi, elf 2005) -- work in progress.
 *
 * Uses the Nonomura Byouin (2003) script format: MES files start with an
 * address table, subroutines are defined in LIBLARY.LIB, and there are
 * procedure arguments (arg[n]). Only the 2D mode (HH2D.exe) is targeted.
 */

#include <stdlib.h>
#include <string.h>

#include "nulib.h"
#include "nulib/little_endian.h"
#include "ai5/arc.h"
#include "ai5/cg.h"
#include "ai5/game.h"
#include "ai5/mes.h"

#include "ai5.h"
#include "asset.h"
#include "audio.h"
#include "encoding.h"
#include "game.h"
#include "gfx.h"
#include "gfx_private.h"
#include "memory.h"
#include "sys.h"
#include "vm.h"
#include "input.h"
#include "cursor.h"
#include "vm_private.h"

// mem16 layout (from HH2D.exe)
#define MES_NAME_SIZE 0x80
#define VAR4_SIZE     0x2000
#define VAR4_OFF      MES_NAME_SIZE              // 0x0080
#define SV16_PTR_OFF  (VAR4_OFF + VAR4_SIZE)     // 0x2080
#define VAR16_OFF     (SV16_PTR_OFF + 4)         // 0x2084
#define SYSVAR16_OFF  (VAR16_OFF + 26 * 2)       // 0x20b8
#define VAR32_OFF     (SYSVAR16_OFF + 26 * 2)    // 0x20ec
#define SYSVAR32_OFF  (VAR32_OFF + 26 * 4)       // 0x2154
#define HEAP_OFF      (SYSVAR32_OFF + 26 * 4)    // 0x21bc
#define ARGS_OFF      0x4000
#define MEM16_SIZE    (ARGS_OFF + VM_MAX_ARGS * 4)
_Static_assert(SYSVAR16_OFF == 0x20b8);
_Static_assert(SYSVAR32_OFF == 0x2154);
_Static_assert(MEM16_SIZE <= MEMORY_MEM16_MAX_SIZE);

static uint32_t param_or(struct param_list *params, unsigned i, uint32_t def)
{
	if (i >= params->nr_params || params->params[i].type != MES_PARAM_EXPRESSION)
		return def;
	return params->params[i].val;
}

// results of system calls are returned in var32[18] (mem16 + 0x2134)
static void set_result(uint32_t v)
{
	mem_set_var32(18, v);
}

// log each unknown system call once per distinct (function, first param)
static void log_unimplemented(const char *what, unsigned no, struct param_list *params)
{
	// log each distinct (what, no, first param) only once
	static uint32_t seen[1024];
	static unsigned nr_seen = 0;
	unsigned first = (params->nr_params && params->params[0].type == MES_PARAM_EXPRESSION)
		? params->params[0].val : 0xffff;
	uint32_t key = (what[0] << 24) ^ (no << 12) ^ (first & 0xfff);
	for (unsigned i = 0; i < nr_seen; i++) {
		if (seen[i] == key)
			return;
	}
	if (nr_seen < 1024)
		seen[nr_seen++] = key;
	string s = string_new("");
	for (unsigned i = 0; i < params->nr_params; i++) {
		if (i)
			s = string_concat_cstring(s, ",");
		if (params->params[i].type == MES_PARAM_STRING)
			s = string_concat_fmt(s, "\"%s\"", params->params[i].str);
		else
			s = string_concat_fmt(s, "%u", params->params[i].val);
	}
	WARNING("hanahebi: %s[%u](%s) not implemented", what, no, s);
	string_free(s);
}

static void msgwin_show(void);
static void msgwin_hide(void);

#define SYS_STUB(n) \
	static void sys_stub_##n(struct param_list *params) \
	{ \
		log_unimplemented("System.function", n, params); \
	}
SYS_STUB(3) SYS_STUB(4)
SYS_STUB(10)
SYS_STUB(14) SYS_STUB(16) SYS_STUB(20)
SYS_STUB(23) SYS_STUB(24) SYS_STUB(26) SYS_STUB(28)

/*
 * Character sprites (Util 50, HH2D.exe 0x429xxx/0x42axxx).
 * Three slots: 0 = center, 1 = left (-160px), 2 = right (+160px).
 * Sprites are 32-bit CGs with alpha, drawn over the background in surface 1.
 */
#define NR_CHAR_SLOTS 3
#define WORK_SURFACE  13
#define FADE_SURFACE  14
// each slot has a body sprite and an optional expression (face) sprite
static struct cg *char_cg[NR_CHAR_SLOTS];
static struct cg *char_face[NR_CHAR_SLOTS];

static void char_unload_face(unsigned slot)
{
	if (slot < NR_CHAR_SLOTS && char_face[slot]) {
		cg_free(char_face[slot]);
		char_face[slot] = NULL;
	}
}

static void char_unload(unsigned slot)
{
	if (slot < NR_CHAR_SLOTS && char_cg[slot]) {
		cg_free(char_cg[slot]);
		char_cg[slot] = NULL;
	}
	char_unload_face(slot);
}

static struct cg *char_load_cg(const char *name, unsigned slot)
{
	if (slot >= NR_CHAR_SLOTS) {
		WARNING("Invalid character slot: %u", slot);
		return NULL;
	}
	struct cg *cg = asset_cg_load(name);
	if (!cg) {
		WARNING("Failed to load character CG \"%s\"", name);
		return NULL;
	}
	if (slot == 1)
		cg->metrics.x -= 160;
	else if (slot == 2)
		cg->metrics.x += 160;
	return cg;
}

static void char_load(const char *name, unsigned slot)
{
	struct cg *cg = char_load_cg(name, slot);
	if (!cg)
		return;
	char_unload(slot);
	char_cg[slot] = cg;
	mem_set_sysvar16(mes_sysvar16_cg_x, cg->metrics.x);
	mem_set_sysvar16(mes_sysvar16_cg_y, cg->metrics.y);
	mem_set_sysvar16(mes_sysvar16_cg_w, cg->metrics.w);
	mem_set_sysvar16(mes_sysvar16_cg_h, cg->metrics.h);
}

// crossfade the screen to the contents of `src`
static void crossfade_to(unsigned src)
{
	unsigned step = mem_get_var16(5);
	if (!step || step >= 64) {
		gfx_copy(0, 0, 640, 480, src, 0, 0, 0);
		return;
	}
	gfx_copy(0, 0, 640, 480, 0, 0, 0, FADE_SURFACE);
	vm_timer_t timer = vm_timer_create();
	for (unsigned a = step; a < 256; a += step) {
		if (input_down(INPUT_CTRL))
			break;
		gfx_copy(0, 0, 640, 480, FADE_SURFACE, 0, 0, 0);
		gfx_blend(0, 0, 640, 480, src, 0, 0, 0, a);
		vm_peek();
		vm_timer_tick(&timer, 16);
	}
	gfx_copy(0, 0, 640, 480, src, 0, 0, 0);
}

// redraw background (surface 1) + all characters, then show it
static void char_present(bool fade)
{
	gfx_copy(0, 0, 640, 480, 1, 0, 0, WORK_SURFACE);
	for (unsigned i = 0; i < NR_CHAR_SLOTS; i++) {
		if (char_cg[i])
			gfx_draw_cg(WORK_SURFACE, char_cg[i]);
		if (char_face[i])
			gfx_draw_cg(WORK_SURFACE, char_face[i]);
	}
	if (fade)
		crossfade_to(WORK_SURFACE);
	else
		gfx_copy(0, 0, 640, 480, WORK_SURFACE, 0, 0, 0);
}

static void util_characters(struct param_list *params)
{
	unsigned op = param_or(params, 1, 0);
	switch (op) {
	case 0: // remove all
		for (unsigned i = 0; i < NR_CHAR_SLOTS; i++)
			char_unload(i);
		break;
	case 1: // load (1, name, slot)
		char_load(vm_string_param(params, 2), param_or(params, 3, 0));
		break;
	case 2: // remove (2, slot)
		char_unload(param_or(params, 2, 0));
		break;
	case 3: // remove (3, slot, fade) and redraw
		char_unload(param_or(params, 2, 0));
		char_present(param_or(params, 3, 0) == 1);
		break;
	case 4: // redraw (4, _, fade)
		char_present(param_or(params, 3, 0) == 1);
		break;
	case 5:
	case 6: // redraw with fade
		char_present(true);
		break;
	case 8:
	case 9: // show (8, slot, fade)
		char_present(param_or(params, 3, 0) == 1);
		break;
	case 10:
	case 11: // redraw
		char_present(false);
		break;
	case 18: { // change expression (18, _, name, slot)
		unsigned slot = param_or(params, 4, 0);
		struct cg *face = char_load_cg(vm_string_param(params, 3), slot);
		if (face) {
			char_unload_face(slot);
			char_face[slot] = face;
			char_present(false);
		}
		break;
	}
	case 12: case 14: case 22: // TODO: lip sync / face animation
		break;
	default:
		log_unimplemented("Util", 50, params);
	}
}

/*
 * Util 51: draw a character sprite directly onto the background (surface 1).
 *   (0, name, slot), (1, name, slot), (3/4) clear
 * The drawn rectangle is returned in var16[23..26].
 */
static void util_characters_direct(struct param_list *params)
{
	switch (param_or(params, 1, 0)) {
	case 0:
	case 1: {
		struct cg *cg = char_load_cg(vm_string_param(params, 2), param_or(params, 3, 0));
		if (!cg)
			break;
		gfx_draw_cg(1, cg);
		mem_set_var16(23, cg->metrics.x);
		mem_set_var16(24, cg->metrics.y);
		mem_set_var16(25, cg->metrics.x + cg->metrics.w);
		mem_set_var16(26, cg->metrics.y + cg->metrics.h);
		cg_free(cg);
		break;
	}
	case 2:
	case 3:
	case 4:
		break;
	default:
		log_unimplemented("Util", 51, params);
	}
}

static void hanahebi_util(struct param_list *params)
{
	switch (vm_expr_param(params, 0)) {
	case 50:
		util_characters(params);
		break;
	case 51:
		util_characters_direct(params);
		break;
	case 23: { // transition speed from AI5WIN.INI [DispEffect] Play -> var16[5]
		const char *v = config_ini_get("DispEffect", "Play");
		static const uint16_t speed[] = { 8, 16, 32, 64 };
		unsigned i = v ? atoi(v) : 1;
		mem_set_var16(5, speed[i < 4 ? i : 1]);
		break;
	}
	case 502: msgwin_show(); break;
	case 503: msgwin_hide(); break;
	default:
		log_unimplemented("Util", vm_expr_param(params, 0), params);
	}
}

// Util numbers go far beyond GAME_MAX_UTIL (e.g. 503, 12000)
static void hanahebi_stmt_util(void)
{
	struct param_list params = {0};
	game->vm.read_params(&params);
	if (unlikely(params.nr_params < 1))
		VM_ERROR("Util without parameters");
	hanahebi_util(&params);
}

/*
 * System.function[8]: load files
 *   (0, name, offset)    load data file into file_data + offset
 *   (2/3, name)          load a MES library (e.g. LIBLARY.LIB) into its own
 *                        buffer and execute it (defines subroutines)
 */
static void hanahebi_load(struct param_list *params)
{
	switch (vm_expr_param(params, 0)) {
	case 0:
	case 1:
		vm_load_data_file(vm_string_param(params, 1), param_or(params, 2, 0));
		break;
	case 2:
	case 3: {
		const char *name = vm_string_param(params, 1);
		struct archive_data *file = asset_mes_load(name);
		if (!file)
			VM_ERROR("Failed to load library \"%s\"", name);
		uint32_t table_size = file->size >= 4 ? 4 + le_get32(file->data, 0) * 4 : 0;
		if (table_size > file->size)
			VM_ERROR("Invalid library \"%s\"", name);
		size_t code_size = file->size - table_size;
		// never freed: subroutines keep pointing into this buffer
		uint8_t *code = xmalloc(code_size + 1);
		memcpy(code, file->data + table_size, code_size);
		code[code_size] = 0;
		archive_data_release(file);
		struct vm_pointer saved_ip = vm.ip;
		vm.ip.code = code;
		vm.ip.ptr = 0;
		game->vm.exec();
		vm.ip = saved_ip;
		break;
	}
	default:
		log_unimplemented("System.function", 8, params);
	}
}

// System.function[0](): set font size from sysvar16[12],[13]
static void hanahebi_set_font_size(struct param_list *params)
{
	gfx_text_set_size(mem_get_sysvar16(mes_sysvar16_font_height), 0);
}

/*
 * System.function[11]: graphics operations (HH2D.exe 0x46ba00 / 0x450ab0)
 *   (op, src_x, src_y, src_br_x, src_br_y, src_i, dst_x, dst_y, dst_i, ...)
 */
static uint32_t fill_color = 0;
static uint32_t fill_color2 = 0;

static void hanahebi_graphics(struct param_list *params)
{
	unsigned op = vm_expr_param(params, 0);
	int sx = (int16_t)param_or(params, 1, 0);
	int sy = (int16_t)param_or(params, 2, 0);
	int w = ((int16_t)param_or(params, 3, 0) - sx) + 1;
	int h = ((int16_t)param_or(params, 4, 0) - sy) + 1;
	unsigned src_i = param_or(params, 5, 0);
	int dx = (int16_t)param_or(params, 6, 0);
	int dy = (int16_t)param_or(params, 7, 0);
	unsigned dst_i = param_or(params, 8, 0);
	switch (op) {
	case 0: // copy
		gfx_copy(sx, sy, w, h, src_i, dx, dy, dst_i);
		break;
	case 1: // copy with transparent color
		gfx_copy_masked(sx, sy, w, h, src_i, dx, dy, dst_i,
				mem_get_sysvar32(mes_sysvar32_mask_color));
		break;
	case 2: // fill rect on dst_surface
		gfx_fill(sx, sy, w, h, mem_get_sysvar16(mes_sysvar16_dst_surface), fill_color);
		break;
	case 6: { // blend: (6, ax, ay, abr_x, abr_y, a_i, bx, by, b_i, dst_x, dst_y, _, alpha)
		unsigned b_i = dst_i;
		int out_x = (int16_t)param_or(params, 9, 0);
		int out_y = (int16_t)param_or(params, 10, 0);
		unsigned alpha = param_or(params, 12, 255);
		gfx_copy(dx, dy, w, h, b_i, out_x, out_y, 0);
		gfx_blend(sx, sy, w, h, src_i, out_x, out_y, 0, min(alpha, 255));
		break;
	}
	default:
		log_unimplemented("System.function", 11, params);
	}
}

// System.function[13](bg, fg): fill color and text color
static void hanahebi_set_colors(struct param_list *params)
{
	fill_color = param_or(params, 0, 0);
	fill_color2 = param_or(params, 1, 0);
	gfx_text_set_colors(fill_color, fill_color2);
}

/*
 * Message window. Text is drawn on surface 9 (640x128, transparent color =
 * fill color 0) and shown at the bottom of the screen over a darkened box.
 * Util(502) shows it, Util(503) hides it.
 * TODO: exact behavior of HH2D.exe (object at +0x21778)
 */
#define MSGWIN_SURFACE 9
#define MSGWIN_Y       352
#define MSGWIN_H       128
static bool msgwin_visible = false;

// draw the message window into overlay 0 (shown on top of the screen)
static void msgwin_compose(void)
{
	const char *a = config_ini_get("Mes", "nAlpha");
	unsigned alpha = a ? atoi(a) : 128;
	SDL_Surface *ov = gfx_get_overlay(0);
	SDL_CALL(SDL_FillRect, ov, NULL, SDL_MapRGBA(ov->format, 0, 0, 0, 0));
	SDL_Rect r = { 0, MSGWIN_Y, 640, MSGWIN_H };
	SDL_CALL(SDL_FillRect, ov, &r, SDL_MapRGBA(ov->format, 0, 0, 0, alpha));
	SDL_Surface *text = gfx_get_surface(MSGWIN_SURFACE);
	SDL_Rect src = { 0, 0, 640, MSGWIN_H };
	SDL_CALL(SDL_SetColorKey, text, SDL_TRUE, SDL_MapRGB(text->format, 0, 0, 0));
	SDL_CALL(SDL_BlitSurface, text, &src, ov, &r);
	SDL_CALL(SDL_SetColorKey, text, SDL_FALSE, 0);
	gfx_overlay_enable(0);
	gfx_screen_dirty();
}

static void msgwin_show(void)
{
	if (getenv("HH_DEBUG_TEXT")) NOTICE("msgwin show");
	msgwin_visible = true;
	msgwin_compose();
}

static void msgwin_hide(void)
{
	if (getenv("HH_DEBUG_TEXT")) NOTICE("msgwin hide");
	msgwin_visible = false;
	gfx_overlay_disable(0);
	gfx_screen_dirty();
}

static void hanahebi_draw_text(const char *text)
{
	if (getenv("HH_DEBUG_TEXT"))
		NOTICE("text dst=%u cur=%u,%u start=%u,%u end=%u,%u space=%u,%u font=%u",
			mem_get_sysvar16(mes_sysvar16_dst_surface),
			mem_get_sysvar16(mes_sysvar16_text_cursor_x), mem_get_sysvar16(mes_sysvar16_text_cursor_y),
			mem_get_sysvar16(mes_sysvar16_text_start_x), mem_get_sysvar16(mes_sysvar16_text_start_y),
			mem_get_sysvar16(mes_sysvar16_text_end_x), mem_get_sysvar16(mes_sysvar16_text_end_y),
			mem_get_sysvar16(mes_sysvar16_char_space), mem_get_sysvar16(mes_sysvar16_line_space),
			mem_get_sysvar16(mes_sysvar16_font_height));
	vm_draw_text(text, 1);
	if (msgwin_visible && mem_get_sysvar16(mes_sysvar16_dst_surface) == MSGWIN_SURFACE)
		msgwin_compose();
}

// System.function[12](): wait for click; [12](ms): wait
static void hanahebi_wait(struct param_list *params)
{
	if (params->nr_params == 0) {
		sys_wait(params);
		return;
	}
	vm_timer_t timer = vm_timer_create();
	vm_timer_t target = timer + vm_expr_param(params, 0);
	while (timer < target && !input_down(INPUT_CTRL)) {
		vm_peek();
		vm_timer_tick(&timer, min(target - timer, 8));
	}
}

// System.function[19]: message history (backlog). Not implemented yet;
// pretend the log is empty.
static void hanahebi_backlog(struct param_list *params)
{
	set_result(0);
}

/*
 * Key numbers used by System.function[18] and [12] (HH2D.exe 0x415210).
 * 0=enter/left click, 1=esc/right click, 2..5=up/down/left/right,
 * 6=shift, 7=ctrl, 8=space, 9=backspace, 10=page up, 11=page down
 */
static bool key_down(unsigned key)
{
	switch (key) {
	case 0:  return input_down(INPUT_ACTIVATE);
	case 1:  return input_down(INPUT_CANCEL);
	case 2:  return input_down(INPUT_UP);
	case 3:  return input_down(INPUT_DOWN);
	case 4:  return input_down(INPUT_LEFT);
	case 5:  return input_down(INPUT_RIGHT);
	case 6:  return input_down(INPUT_SHIFT);
	case 7:  return input_down(INPUT_CTRL);
	case 8:  return input_down(INPUT_SPACE);
	case 9:  return input_down(INPUT_BACKSPACE);
	case 10: return input_down(INPUT_PAGE_UP);
	case 11: return input_down(INPUT_PAGE_DOWN);
	}
	return false;
}

// System.function[18](key, state): var32[18] = (key pressed == state)
static void hanahebi_check_input(struct param_list *params)
{
	handle_events();
	set_result(key_down(vm_expr_param(params, 0)) == !!param_or(params, 1, 1));
}

// System.function[2]: mouse cursor
static void hanahebi_cursor(struct param_list *params)
{
	switch (vm_expr_param(params, 0)) {
	case 0: cursor_show(); break;
	case 1: cursor_hide(); break;
	case 2: {
		unsigned x, y;
		cursor_get_pos(&x, &y);
		mem_set_sysvar16(mes_sysvar16_cursor_x, x);
		mem_set_sysvar16(mes_sysvar16_cursor_y, y);
		break;
	}
	case 3: cursor_set_pos(param_or(params, 1, 0), param_or(params, 2, 0)); break;
	case 5: // mouse wheel: var16[18] = direction, var16[25] = moved
		mem_set_var16(18, 0);
		mem_set_var16(25, 0);
		break;
	default:
		log_unimplemented("System.function", 2, params);
	}
}

/*
 * System.function[27]: game settings stored in AI5WIN.INI
 *   (0, section, key)         var16[18] = value
 *   (1, section, key, value)  set value
 */
static void hanahebi_settings(struct param_list *params)
{
	const char *section = vm_string_param(params, 1);
	const char *key = vm_string_param(params, 2);
	if (vm_expr_param(params, 0) == 0) {
		const char *v = config_ini_get(section, key);
		mem_set_var16(18, v ? atoi(v) : 0);
	} else {
		char buf[16];
		snprintf(buf, sizeof(buf), "%u", (unsigned)param_or(params, 3, 0));
		config_ini_set(section, key, buf);
	}
}

// System.function[29](): var32[18] = time in milliseconds
static void hanahebi_get_time(struct param_list *params)
{
	set_result(vm_get_ticks());
}

/*
 * menuexec(n): procedures 0-9 are defined by the script before each menu.
 *   8 = init, 9 = update, 2 = activate, 3 = cancel, 4-7 = up/down/left/right.
 * The menu ends when System.flags & 0x4 is set.
 */
static void hanahebi_menuexec(void)
{
	struct param_list params = {0};
	game->vm.read_params(&params);
	vm_call_procedure(8);
	while (!vm_flag_is_on(FLAG_MENU_RETURN) && !vm_flag_is_on(FLAG_RETURN)) {
		vm_call_procedure(9);
		if (vm_flag_is_on(FLAG_MENU_RETURN))
			break;
		if (input_down(INPUT_ACTIVATE)) {
			vm_call_procedure(2);
			input_wait_until_up(INPUT_ACTIVATE);
		} else if (input_down(INPUT_CANCEL)) {
			vm_call_procedure(3);
			input_wait_until_up(INPUT_CANCEL);
		} else if (input_down(INPUT_UP)) {
			vm_call_procedure(4);
			input_wait_until_up(INPUT_UP);
		} else if (input_down(INPUT_DOWN)) {
			vm_call_procedure(5);
			input_wait_until_up(INPUT_DOWN);
		} else if (input_down(INPUT_LEFT)) {
			vm_call_procedure(6);
			input_wait_until_up(INPUT_LEFT);
		} else if (input_down(INPUT_RIGHT)) {
			vm_call_procedure(7);
			input_wait_until_up(INPUT_RIGHT);
		} else {
			vm_peek();
			vm_delay(16);
		}
	}
}

/*
 * Audio (HH2D.exe 0x44fcd0 BGM, 0x44ffb0 SE, 0x450170 voice).
 *   BGM:   (0,name[,ch]) play, (1[,ch]) stop, (2[,ch]) fade out,
 *          (3,name[,ch]) queue, (4[,ch]) play queued
 *   SE:    (0,name,ch) play, (1,ch) stop, (2,ch) fade out, (3,ch) -> playing?
 *   Voice: (0,name[,ch]) play, (1[,ch]) stop, (2,name[,ch]) queue,
 *          (3[,ch]) play queued, (4[,ch]) -> playing?, (5[,ch]) -> queued?
 * Results go to var32[18].
 */
static char bgm_queued[STRING_PARAM_SIZE];
static char voice_queued[STRING_PARAM_SIZE];
static bool voice_pending = false;

static bool setting_enabled(const char *section, const char *key)
{
	const char *v = config_ini_get(section, key);
	return !v || atoi(v) != 0;
}

static void hanahebi_bgm(struct param_list *params)
{
	switch (vm_expr_param(params, 0)) {
	case 0:
		if (setting_enabled("Music", "IsMusic"))
			audio_bgm_play(vm_string_param(params, 1), false);
		break;
	case 1:
		audio_stop(AUDIO_CH_BGM);
		break;
	case 2:
		audio_fade(AUDIO_CH_BGM, AUDIO_VOLUME_MIN, 1000, true, false);
		break;
	case 3:
		strncpy(bgm_queued, vm_string_param(params, 1), sizeof(bgm_queued) - 1);
		break;
	case 4:
		if (bgm_queued[0] && setting_enabled("Music", "IsMusic"))
			audio_bgm_play(bgm_queued, false);
		break;
	default:
		log_unimplemented("System.function", 5, params);
	}
}

static unsigned se_channel(struct param_list *params, unsigned i)
{
	unsigned ch = param_or(params, i, 0);
	return audio_se_channel_valid(ch) ? ch : 0;
}

static void hanahebi_se(struct param_list *params)
{
	switch (vm_expr_param(params, 0)) {
	case 0:
		if (setting_enabled("Se", "IsSe"))
			audio_se_play(vm_string_param(params, 1), se_channel(params, 2));
		break;
	case 1:
		audio_se_stop(se_channel(params, 1));
		break;
	case 2:
		audio_se_fade(AUDIO_VOLUME_MIN, 1000, true, false, se_channel(params, 1));
		break;
	case 3:
		set_result(audio_is_playing(AUDIO_CH_SE(se_channel(params, 1))));
		break;
	default:
		log_unimplemented("System.function", 6, params);
	}
}

static void hanahebi_voice(struct param_list *params)
{
	switch (vm_expr_param(params, 0)) {
	case 0:
		if (setting_enabled("Voice", "IsVoice"))
			audio_voice_play(vm_string_param(params, 1), 0);
		break;
	case 1:
		audio_voice_stop(0);
		voice_pending = false;
		break;
	case 2:
		strncpy(voice_queued, vm_string_param(params, 1), sizeof(voice_queued) - 1);
		voice_pending = true;
		break;
	case 3:
		if (voice_pending) {
			audio_voice_stop(0);
			if (setting_enabled("Voice", "IsVoice"))
				audio_voice_play(voice_queued, 0);
		}
		voice_pending = false;
		break;
	case 4:
		set_result(audio_is_playing(AUDIO_CH_VOICE0));
		break;
	case 5:
		set_result(voice_pending);
		break;
	default:
		log_unimplemented("System.function", 7, params);
	}
}

// System.function[9](name): load a GCC image into the destination surface
static void hanahebi_load_image(struct param_list *params)
{
	sys_load_image(params);
}

static void hanahebi_mem_restore(void)
{
	mem_set_sysvar16_ptr(SYSVAR16_OFF);
	mem_set_sysvar32(mes_sysvar32_memory, offsetof(struct memory, mem16));
	mem_set_sysvar32(mes_sysvar32_file_data, offsetof(struct memory, file_data));
	mem_set_sysvar32(mes_sysvar32_menu_entry_addresses,
		offsetof(struct memory, menu_entry_addresses));
	mem_set_sysvar32(mes_sysvar32_menu_entry_numbers,
		offsetof(struct memory, menu_entry_numbers));
	mem_set_sysvar16(0, HEAP_OFF);
}

static void hanahebi_mem_init(void)
{
	memory_ptr.mes_name = memory_raw;
	memory_ptr.var4 = memory_raw + VAR4_OFF;
	memory_ptr.system_var16_ptr = memory_raw + SV16_PTR_OFF;
	memory_ptr.var16 = memory_raw + VAR16_OFF;
	memory_ptr.system_var16 = memory_raw + SYSVAR16_OFF;
	memory_ptr.var32 = memory_raw + VAR32_OFF;
	memory_ptr.system_var32 = memory_raw + SYSVAR32_OFF;
	vm_args = (uint32_t*)(memory_raw + ARGS_OFF);

	mem_set_sysvar16(mes_sysvar16_text_start_x, 0);
	mem_set_sysvar16(mes_sysvar16_text_start_y, 0);
	mem_set_sysvar16(mes_sysvar16_text_end_x, 640);
	mem_set_sysvar16(mes_sysvar16_text_end_y, 480);
	mem_set_sysvar32(mes_sysvar32_cg_offset, 0x20000);
	hanahebi_mem_restore();
}

static void hanahebi_init(void)
{
	// sound effects are in their own archive (se.arc)
	asset_effect_is_bgm = false;
}

struct game game_hanahebi = {
	.id = GAME_HANAHEBI,
	.surface_sizes = {
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 640, 480 },
		{ 0, 0 }
	},
	.bpp = 24,
	.var4_size = VAR4_SIZE,
	.mem16_size = MEM16_SIZE,
	.mem_init = hanahebi_mem_init,
	.mem_restore = hanahebi_mem_restore,
	.init = hanahebi_init,
	.draw_text_zen = hanahebi_draw_text,
	.draw_text_han = hanahebi_draw_text,
	.vm = VM_AI5,
	.expr_op = {
		DEFAULT_EXPR_OP,
		[0xf3] = vm_expr_cflag,
		[0xf5] = vm_expr_eflag,
		[0xf6] = vm_expr_arg_const,
		[0xf7] = vm_expr_arg_expr,
		[0xf8] = vm_expr_var32,
		[0xf9] = vm_expr_ptr32_get32,
	},
	.stmt_op = {
		[0x01] = vm_stmt_txt_no_log,
		[0x02] = vm_stmt_str_no_log,
		[0x03] = vm_stmt_set_flag_const16,
		[0x04] = vm_stmt_set_var16_const8,
		[0x05] = vm_stmt_set_flag_expr,
		[0x06] = vm_stmt_ptr16_set8,
		[0x07] = vm_stmt_ptr16_set16,
		[0x08] = vm_stmt_ptr32_set32,
		[0x09] = vm_stmt_set_arg_const,
		[0x0a] = vm_stmt_set_arg_expr,
		[0x0b] = vm_stmt_jz,
		[0x0c] = vm_stmt_jmp,
		[0x0d] = vm_stmt_sys,
		[0x0e] = vm_stmt_mesjmp,
		[0x0f] = vm_stmt_mescall,
		[0x10] = vm_stmt_defmenu,
		[0x11] = vm_stmt_call,
		[0x12] = hanahebi_stmt_util,
		[0x13] = vm_stmt_line,
		[0x14] = vm_stmt_defproc,
		[0x15] = hanahebi_menuexec,
		[0x16] = vm_stmt_set_var32_const8,
		[0x17] = vm_stmt_skip_dword,
		[0x18] = vm_stmt_skip_expr,
		[0x19] = vm_stmt_nop,
		[0x1a] = vm_stmt_nop,
		[0x1b] = vm_stmt_skip_params,
		[0x1c] = vm_stmt_defsub,
		[0x1d] = vm_stmt_callsub,
		[0x1f] = vm_stmt_skip_dword,
	},
	.sys = {
		[0]  = hanahebi_set_font_size,
		[2]  = hanahebi_cursor,
		[3]  = sys_stub_3,
		[4]  = sys_stub_4,
		[5]  = hanahebi_bgm,
		[6]  = hanahebi_se,
		[7]  = hanahebi_voice,
		[8]  = hanahebi_load,
		[9]  = hanahebi_load_image,
		[10] = sys_stub_10,
		[11] = hanahebi_graphics,
		[12] = hanahebi_wait,
		[13] = hanahebi_set_colors,
		[14] = sys_stub_14,
		[16] = sys_stub_16,
		[18] = hanahebi_check_input,
		[19] = hanahebi_backlog,
		[20] = sys_stub_20,
		[23] = sys_stub_23,
		[24] = sys_stub_24,
		[26] = sys_stub_26,
		[27] = hanahebi_settings,
		[28] = sys_stub_28,
		[29] = hanahebi_get_time,
	},
	.util = {
		[0 ... GAME_MAX_UTIL-1] = hanahebi_util,
	},
	.flags = {
		// not a real flag in this game (0x10 is used by scripts for something else)
		[FLAG_RETURN] = 0x8000,
		[FLAG_MENU_RETURN] = 0x0004,
	},
};
