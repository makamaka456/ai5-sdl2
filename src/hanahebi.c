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
#include "vm_private.h"

#define MES_NAME_SIZE 128
#define VAR4_SIZE     0x2000
#define VAR16_COUNT   512
#define SYSVAR16_COUNT 64
#define VAR32_COUNT   64
#define SYSVAR32_COUNT 64

#define VAR4_OFF     MES_NAME_SIZE
#define SV16_PTR_OFF (VAR4_OFF + VAR4_SIZE)
#define VAR16_OFF    (SV16_PTR_OFF + 4)
#define SYSVAR16_OFF (VAR16_OFF + VAR16_COUNT * 2)
#define VAR32_OFF    (SYSVAR16_OFF + SYSVAR16_COUNT * 2)
#define SYSVAR32_OFF (VAR32_OFF + VAR32_COUNT * 4)
#define HEAP_OFF     (SYSVAR32_OFF + SYSVAR32_COUNT * 4)
#define MEM16_SIZE   0x4000
_Static_assert(HEAP_OFF < MEM16_SIZE);
_Static_assert(MEM16_SIZE <= MEMORY_MEM16_MAX_SIZE);

// log each unknown system call once per distinct (function, first param)
static void log_unimplemented(const char *what, unsigned no, struct param_list *params)
{
	static uint8_t seen[GAME_MAX_SYS][16];
	unsigned sub = (params->nr_params && params->params[0].type == MES_PARAM_EXPRESSION)
		? params->params[0].val : 0;
	if (no < GAME_MAX_SYS && sub < 16) {
		if (seen[no][sub])
			return;
		seen[no][sub] = 1;
	}
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
SYS_STUB(0) SYS_STUB(2) SYS_STUB(3) SYS_STUB(4) SYS_STUB(5) SYS_STUB(6)
SYS_STUB(7) SYS_STUB(8) SYS_STUB(10) SYS_STUB(11) SYS_STUB(12)
SYS_STUB(13) SYS_STUB(14) SYS_STUB(16) SYS_STUB(18) SYS_STUB(19) SYS_STUB(20)
SYS_STUB(23) SYS_STUB(24) SYS_STUB(26) SYS_STUB(27) SYS_STUB(28) SYS_STUB(29)

static void hanahebi_util(struct param_list *params)
{
	log_unimplemented("Util", vm_expr_param(params, 0), params);
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

	mem_set_sysvar16(mes_sysvar16_text_start_x, 0);
	mem_set_sysvar16(mes_sysvar16_text_start_y, 0);
	mem_set_sysvar16(mes_sysvar16_text_end_x, 640);
	mem_set_sysvar16(mes_sysvar16_text_end_y, 480);
	mem_set_sysvar32(mes_sysvar32_cg_offset, 0x20000);
	hanahebi_mem_restore();
}

static void hanahebi_init(void)
{
	// subroutines used by all scripts are defined in LIBLARY.LIB
	struct vm_pointer saved_ip = vm.ip;
	// copy library code out of file_data so later MES loads don't clobber it
	static uint8_t *lib_code = NULL;
	struct archive_data *file = asset_mes_load("LIBLARY.LIB");
	if (!file)
		VM_ERROR("Failed to load LIBLARY.LIB");
	uint32_t table_size = 4 + le_get32(file->data, 0) * 4;
	size_t code_size = file->size - table_size;
	lib_code = xmalloc(code_size + 1);
	memcpy(lib_code, file->data + table_size, code_size);
	lib_code[code_size] = 0;
	archive_data_release(file);
	vm.ip.code = lib_code;
	vm.ip.ptr = 0;
	game->vm.exec();
	vm.ip = saved_ip;
	NOTICE("hanahebi: LIBLARY.LIB loaded");
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
		[0x12] = vm_stmt_util,
		[0x13] = vm_stmt_line,
		[0x14] = vm_stmt_defproc,
		[0x15] = vm_stmt_menuexec_params,
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
		[0]  = sys_stub_0,
		[2]  = sys_stub_2,
		[3]  = sys_stub_3,
		[4]  = sys_stub_4,
		[5]  = sys_stub_5,
		[6]  = sys_stub_6,
		[7]  = sys_stub_7,
		[8]  = sys_stub_8,
		[9]  = hanahebi_load_image,
		[10] = sys_stub_10,
		[11] = sys_stub_11,
		[12] = sys_stub_12,
		[13] = sys_stub_13,
		[14] = sys_stub_14,
		[16] = sys_stub_16,
		[18] = sys_stub_18,
		[19] = sys_stub_19,
		[20] = sys_stub_20,
		[23] = sys_stub_23,
		[24] = sys_stub_24,
		[26] = sys_stub_26,
		[27] = sys_stub_27,
		[28] = sys_stub_28,
		[29] = sys_stub_29,
	},
	.util = {
		[0 ... GAME_MAX_UTIL-1] = hanahebi_util,
	},
	.flags = {
		// not a real flag in this game (0x10 is used by scripts for something else)
		[FLAG_RETURN] = 0x8000,
	},
};
