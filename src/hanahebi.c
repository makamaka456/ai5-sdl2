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
#include "ai5/game.h"
#include "ai5/mes.h"

#include "ai5.h"
#include "asset.h"
#include "audio.h"
#include "encoding.h"
#include "game.h"
#include "gfx.h"
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

#define SYS_STUB(n) \
	static void sys_stub_##n(struct param_list *params) \
	{ \
		log_unimplemented("System.function", n, params); \
	}
SYS_STUB(3) SYS_STUB(4) SYS_STUB(5) SYS_STUB(6)
SYS_STUB(7) SYS_STUB(10)
SYS_STUB(14) SYS_STUB(16) SYS_STUB(20)
SYS_STUB(23) SYS_STUB(24) SYS_STUB(26) SYS_STUB(28)

static void hanahebi_util(struct param_list *params)
{
	log_unimplemented("Util", vm_expr_param(params, 0), params);
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

// System.function[13](c1, c2): colors used by fill operations
static void hanahebi_set_colors(struct param_list *params)
{
	fill_color = param_or(params, 0, 0);
	fill_color2 = param_or(params, 1, 0);
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
		{ 0, 0 }
	},
	.bpp = 24,
	.var4_size = VAR4_SIZE,
	.mem16_size = MEM16_SIZE,
	.mem_init = hanahebi_mem_init,
	.mem_restore = hanahebi_mem_restore,
	.init = hanahebi_init,
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
		[5]  = sys_stub_5,
		[6]  = sys_stub_6,
		[7]  = sys_stub_7,
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
