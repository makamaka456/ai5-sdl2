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
#include <strings.h>
#include <time.h>
#include <sys/stat.h>

#include "nulib.h"
#include "nulib/little_endian.h"
#include "nulib/file.h"
#include "savedata.h"
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
#include "menu.h"
#include "cursor.h"
#include "vm_private.h"

/*
 * Load a CG as a private copy. asset_cg_load() returns a CG shared with the
 * CG cache, which must not be modified (we shift positions and edit alpha).
 */
static struct cg *hh_cg_load(const char *name)
{
	struct cg *shared = asset_cg_load(name);
	if (!shared)
		return NULL;
	struct cg *cg;
	if (shared->palette) {
		cg = shared; // indexed CGs are not modified
		return cg;
	}
	cg = cg_alloc_direct(shared->metrics.w, shared->metrics.h);
	cg->metrics = shared->metrics;
	memcpy(cg->pixels, shared->pixels, shared->metrics.w * shared->metrics.h * 4);
	cg_free(shared);
	return cg;
}

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

/*
 * A name parameter. Scripts pass "" plus a memory address (e.g. System.cg_offset,
 * filled by System.function[24](6)) when the name comes from a screen record.
 */
static const char *name_param(struct param_list *params, unsigned i, unsigned ptr_i)
{
	const char *s = "";
	if (i < params->nr_params && params->params[i].type == MES_PARAM_STRING)
		s = params->params[i].str;
	if (s[0] || ptr_i >= params->nr_params
			|| params->params[ptr_i].type != MES_PARAM_EXPRESSION)
		return s;
	uint32_t off = params->params[ptr_i].val;
	if (off >= sizeof(struct memory) - 32)
		return s;
	return (const char*)memory_raw + off;
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
static void util_save_screen(unsigned mode);
static void util_system_menu(void);

#define SYS_STUB(n) \
	static void sys_stub_##n(struct param_list *params) \
	{ \
		log_unimplemented("System.function", n, params); \
	}
SYS_STUB(3)

SYS_STUB(23) SYS_STUB(28)

/*
 * Character sprites (Util 50, HH2D.exe 0x429xxx/0x42axxx).
 * Three slots: 0 = center, 1 = left (-160px), 2 = right (+160px).
 * Sprites are 32-bit CGs with alpha, drawn over the background in surface 1.
 */
#define NR_CHAR_SLOTS 3
#define BACK_SURFACE  15
#define PREV_SURFACE  16
#define WORK_SURFACE  17
#define FADE_SURFACE  18
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
	struct cg *cg = hh_cg_load(name);
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
		char_load(name_param(params, 2, 4), param_or(params, 3, 0));
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
	case 18: { // change expression (18, _, name, slot); only for Util 50 sprites
		unsigned slot = param_or(params, 4, 0);
		if (slot >= NR_CHAR_SLOTS || !char_cg[slot])
			break;
		struct cg *face = char_load_cg(name_param(params, 3, 5), slot);
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
 * Util 51: character sprites drawn onto the background (surface 1).
 *   (0/1, name, slot)  set sprite of slot and draw it
 *   (2, slot)          remove sprite; surface 1 is rebuilt from the clean
 *                      background in surface 2 plus the remaining sprites
 *   (3/4)              forget all sprites (no redraw)
 * The drawn rectangle is returned in var16[23..26].
 */
static struct cg *direct_cg[NR_CHAR_SLOTS];

static void direct_redraw(void)
{
	gfx_copy(0, 0, 640, 480, 2, 0, 0, 1);
	for (unsigned i = 0; i < NR_CHAR_SLOTS; i++) {
		if (direct_cg[i])
			gfx_draw_cg(1, direct_cg[i]);
	}
}

static void util_characters_direct(struct param_list *params)
{
	switch (param_or(params, 1, 0)) {
	case 0:
	case 1: {
		unsigned slot = param_or(params, 3, 0);
		struct cg *cg = char_load_cg(name_param(params, 2, 4), slot);
		if (!cg)
			break;
		if (direct_cg[slot]) {
			cg_free(direct_cg[slot]);
			direct_cg[slot] = cg;
			direct_redraw();
		} else {
			direct_cg[slot] = cg;
			gfx_draw_cg(1, cg);
		}
		mem_set_var16(23, cg->metrics.x);
		mem_set_var16(24, cg->metrics.y);
		mem_set_var16(25, cg->metrics.x + cg->metrics.w);
		mem_set_var16(26, cg->metrics.y + cg->metrics.h);
		break;
	}
	case 2: {
		unsigned slot = param_or(params, 2, 0);
		if (slot < NR_CHAR_SLOTS && direct_cg[slot]) {
			cg_free(direct_cg[slot]);
			direct_cg[slot] = NULL;
			direct_redraw();
		}
		break;
	}
	case 3:
	case 4:
		for (unsigned i = 0; i < NR_CHAR_SLOTS; i++) {
			if (direct_cg[i]) {
				cg_free(direct_cg[i]);
				direct_cg[i] = NULL;
			}
		}
		break;
	default:
		log_unimplemented("Util", 51, params);
	}
}

/*
 * Alpha planes. HH2D.exe surfaces keep an alpha channel (set by 32-bit CGs
 * and copied by graphics op 8). Our surfaces are 24-bit, so the alpha is kept
 * here and used when buttons are shown on the overlay.
 */
#define SURF_W 640
#define SURF_H 480
static uint8_t *alpha_plane[GFX_NR_SURFACES];

static uint8_t *alpha_get(unsigned i)
{
	if (i >= GFX_NR_SURFACES)
		return NULL;
	if (!alpha_plane[i]) {
		alpha_plane[i] = xmalloc(SURF_W * SURF_H);
		memset(alpha_plane[i], 255, SURF_W * SURF_H);
	}
	return alpha_plane[i];
}

static void alpha_clip(int *x, int *y, int *w, int *h)
{
	if (*x < 0) { *w += *x; *x = 0; }
	if (*y < 0) { *h += *y; *y = 0; }
	if (*x + *w > SURF_W) *w = SURF_W - *x;
	if (*y + *h > SURF_H) *h = SURF_H - *y;
}

static void alpha_copy(int sx, int sy, int w, int h, unsigned src, int dx, int dy, unsigned dst)
{
	uint8_t *s = alpha_get(src), *d = alpha_get(dst);
	if (!s || !d || w <= 0 || h <= 0)
		return;
	for (int row = 0; row < h; row++) {
		int ys = sy + row, yd = dy + row;
		if (ys < 0 || ys >= SURF_H || yd < 0 || yd >= SURF_H)
			continue;
		for (int col = 0; col < w; col++) {
			int xs = sx + col, xd = dx + col;
			if (xs < 0 || xs >= SURF_W || xd < 0 || xd >= SURF_W)
				continue;
			d[yd * SURF_W + xd] = s[ys * SURF_W + xs];
		}
	}
}

static void alpha_fill(int x, int y, int w, int h, unsigned i, uint8_t a)
{
	uint8_t *d = alpha_get(i);
	alpha_clip(&x, &y, &w, &h);
	if (!d || w <= 0 || h <= 0)
		return;
	for (int row = 0; row < h; row++)
		memset(d + (y + row) * SURF_W + x, a, w);
}

/*
 * Buttons (Util 200-203): rectangles copied from a surface and shown on top
 * of the screen (overlay 1) until removed. Used by choice menus.
 *   200(x, y, w, h, id)               define button
 *   201(id)                           remove
 *   202(x, y, w, h, src, _, _, id)    set image (from src surface at x,y)
 *   203(id)                           show
 */
#define NR_BUTTONS 32
static struct { bool defined, visible; int x, y, w, h; unsigned src; } buttons[NR_BUTTONS];

static void buttons_redraw(void)
{
	SDL_Surface *ov = gfx_get_overlay(1);
	SDL_CALL(SDL_FillRect, ov, NULL, SDL_MapRGBA(ov->format, 0, 0, 0, 0));
	bool any = false;
	uint32_t mask = mem_get_sysvar32(mes_sysvar32_mask_color);
	for (int i = 0; i < NR_BUTTONS; i++) {
		if (!buttons[i].defined || !buttons[i].visible)
			continue;
		SDL_Surface *src = gfx_get_surface(buttons[i].src);
		uint8_t *alpha = alpha_get(buttons[i].src);
		int x0 = buttons[i].x, y0 = buttons[i].y, w = buttons[i].w, h = buttons[i].h;
		alpha_clip(&x0, &y0, &w, &h);
		SDL_LockSurface(src);
		SDL_LockSurface(ov);
		for (int y = y0; y < y0 + h; y++) {
			uint8_t *sp = (uint8_t*)src->pixels + y * src->pitch + x0 * src->format->BytesPerPixel;
			uint32_t *dp = (uint32_t*)((uint8_t*)ov->pixels + y * ov->pitch) + x0;
			for (int x = x0; x < x0 + w; x++, sp += src->format->BytesPerPixel, dp++) {
				uint32_t px = 0;
				memcpy(&px, sp, src->format->BytesPerPixel);
				uint8_t r, g, b;
				SDL_GetRGB(px, src->format, &r, &g, &b);
				uint8_t a = alpha[y * SURF_W + x];
				uint32_t m = mask;
				if (r == ((m >> 16) & 0xff) && g == ((m >> 8) & 0xff) && b == (m & 0xff))
					a = 0;
				*dp = SDL_MapRGBA(ov->format, r, g, b, a);
			}
		}
		SDL_UnlockSurface(ov);
		SDL_UnlockSurface(src);
		any = true;
	}
	if (any)
		gfx_overlay_enable(1);
	else
		gfx_overlay_disable(1);
	gfx_screen_dirty();
}

static bool util_buttons(unsigned no, struct param_list *params)
{
	unsigned id;
	switch (no) {
	case 200:
		id = param_or(params, 5, 0);
		if (id >= NR_BUTTONS) return true;
		buttons[id].defined = true;
		buttons[id].visible = false;
		buttons[id].x = param_or(params, 1, 0);
		buttons[id].y = param_or(params, 2, 0);
		buttons[id].w = param_or(params, 3, 0);
		buttons[id].h = param_or(params, 4, 0);
		buttons[id].src = 5;
		return true;
	case 201:
		id = param_or(params, 1, 0);
		if (id < NR_BUTTONS && buttons[id].defined) {
			buttons[id].defined = false;
			buttons_redraw();
		}
		return true;
	case 202:
		id = param_or(params, 8, 0);
		if (id >= NR_BUTTONS) return true;
		buttons[id].defined = true;
		buttons[id].x = param_or(params, 1, 0);
		buttons[id].y = param_or(params, 2, 0);
		buttons[id].w = param_or(params, 3, 0);
		buttons[id].h = param_or(params, 4, 0);
		buttons[id].src = param_or(params, 5, 5);
		if (buttons[id].visible)
			buttons_redraw();
		return true;
	case 203:
		id = param_or(params, 1, 0);
		if (id < NR_BUTTONS && buttons[id].defined) {
			buttons[id].visible = true;
			buttons_redraw();
		}
		return true;
	}
	return false;
}

/*
 * Util(2000): choice dialog of the 2D version (HH2D.exe 0x409790 draws it
 * natively with sel.gcc). Entries are the defmenu entries. Result in var16[18]:
 * menu number + 1, or 0 when cancelled (right click -> system menu).
 * Our version draws simple translucent buttons on overlay 2.
 */
#define CHOICE_MAX   32

static const char *utf8_next(const char *s, int *ch)
{
	const uint8_t *p = (const uint8_t*)s;
	if (p[0] < 0x80) { *ch = p[0]; return s + 1; }
	if ((p[0] & 0xe0) == 0xc0 && p[1]) { *ch = ((p[0] & 0x1f) << 6) | (p[1] & 0x3f); return s + 2; }
	if ((p[0] & 0xf0) == 0xe0 && p[1] && p[2]) {
		*ch = ((p[0] & 0x0f) << 12) | ((p[1] & 0x3f) << 6) | (p[2] & 0x3f);
		return s + 3;
	}
	*ch = '?';
	return s + 1;
}

static unsigned text_width(const char *utf8)
{
	unsigned w = 0;
	while (*utf8) {
		int ch;
		utf8 = utf8_next(utf8, &ch);
		w += gfx_text_size_char(ch);
	}
	return w;
}

struct choice_layout {
	unsigned n, cols, rows;
	int x0, y0, w, h, step_x, step_y;
};

static void choice_rect(struct choice_layout *l, unsigned i, SDL_Rect *r)
{
	unsigned col = i / l->rows, row = i % l->rows;
	r->x = l->x0 + col * l->step_x;
	r->y = l->y0 + row * l->step_y;
	r->w = l->w;
	r->h = l->h;
}

static void choice_draw(struct choice_layout *l, const char **items, int hover)
{
	SDL_Surface *ov = gfx_get_overlay(2);
	SDL_CALL(SDL_FillRect, ov, NULL, SDL_MapRGBA(ov->format, 0, 0, 0, 0));
	gfx_fill(0, 0, 640, 480, WORK_SURFACE, 0);
	gfx_text_set_colors(0, 0xffffff);
	for (unsigned i = 0; i < l->n; i++) {
		SDL_Rect border;
		choice_rect(l, i, &border);
		SDL_Rect inner = { border.x + 3, border.y + 3, border.w - 6, border.h - 6 };
		bool hl = (int)i == hover;
		SDL_CALL(SDL_FillRect, ov, &border, SDL_MapRGBA(ov->format, hl ? 230 : 160, 30, 30, 230));
		SDL_CALL(SDL_FillRect, ov, &inner, SDL_MapRGBA(ov->format, hl ? 70 : 20, 20, 20, hl ? 220 : 180));
		const char *text = items[i];
		int tx = border.x + (border.w - (int)text_width(text)) / 2;
		int ty = border.y + (border.h - 22) / 2;
		while (*text) {
			int ch;
			text = utf8_next(text, &ch);
			tx += gfx_text_draw_glyph(tx, ty, WORK_SURFACE, ch);
		}
		SDL_Surface *ts = gfx_get_surface(WORK_SURFACE);
		SDL_CALL(SDL_SetColorKey, ts, SDL_TRUE, SDL_MapRGB(ts->format, 0, 0, 0));
		SDL_CALL(SDL_BlitSurface, ts, &inner, ov, &inner);
		SDL_CALL(SDL_SetColorKey, ts, SDL_FALSE, 0);
	}
	gfx_overlay_enable(2);
	gfx_screen_dirty();
}

// Show a list of buttons; returns the chosen index, or -1 if cancelled.
static int choice_run(const char **items, unsigned n)
{
	struct choice_layout l = { .n = n };
	gfx_text_set_size(20, 0);
	int tw = 0;
	for (unsigned i = 0; i < n; i++)
		tw = max(tw, (int)text_width(items[i]));
	if (n <= 6) {
		l.cols = 1; l.rows = n;
		l.h = 56; l.step_y = 64;
		l.w = min(max(tw + 96, 320), 600);
	} else {
		l.cols = (n + 9) / 10; l.rows = (n + l.cols - 1) / l.cols;
		l.h = 36; l.step_y = 40;
		l.w = min(max(tw + 40, 180), 600 / (int)l.cols - 8);
	}
	l.step_x = l.w + 8;
	l.x0 = (640 - ((int)l.cols * l.step_x - 8)) / 2;
	l.y0 = (480 - ((int)l.rows * l.step_y - (l.step_y - l.h))) / 2;

	int hover = -1, last = -2, result = -1;
	input_wait_until_up(INPUT_ACTIVATE);
	while (true) {
		unsigned cx, cy;
		cursor_get_pos(&cx, &cy);
		for (unsigned i = 0; i < n; i++) {
			SDL_Rect r;
			choice_rect(&l, i, &r);
			if ((int)cx >= r.x && (int)cx < r.x + r.w && (int)cy >= r.y && (int)cy < r.y + r.h)
				hover = i;
		}
		if (input_down(INPUT_UP)) {
			hover = hover <= 0 ? (int)n - 1 : hover - 1;
			input_wait_until_up(INPUT_UP);
		} else if (input_down(INPUT_DOWN)) {
			hover = hover < 0 || hover >= (int)n - 1 ? 0 : hover + 1;
			input_wait_until_up(INPUT_DOWN);
		}
		if (hover != last) {
			choice_draw(&l, items, hover);
			last = hover;
		}
		if (input_down(INPUT_ACTIVATE)) {
			// the click may come with a mouse move: use the current position
			cursor_get_pos(&cx, &cy);
			for (unsigned i = 0; i < n; i++) {
				SDL_Rect r;
				choice_rect(&l, i, &r);
				if ((int)cx >= r.x && (int)cx < r.x + r.w && (int)cy >= r.y && (int)cy < r.y + r.h)
					hover = i;
			}
			if (hover >= 0) {
				input_wait_until_up(INPUT_ACTIVATE);
				result = hover;
				break;
			}
		}
		if (input_down(INPUT_CANCEL)) {
			input_wait_until_up(INPUT_CANCEL);
			result = -1;
			break;
		}
		vm_peek();
		vm_delay(16);
	}
	gfx_overlay_disable(2);
	gfx_screen_dirty();
	return result;
}

static void util_choice(void)
{
	unsigned n = min(menu_count_entries(), CHOICE_MAX);
	if (!n) {
		mem_set_var16(18, 1);
		return;
	}
	string items[CHOICE_MAX];
	for (unsigned i = 0; i < n; i++) {
		uint32_t addr = memory.menu_entry_addresses[i];
		const char *t = (addr && addr + 1 < sizeof(struct memory)) ? mem_get_cstring(addr + 1) : "";
		items[i] = ai5_cstring_to_utf8(t ? t : "", 0);
	}
	int r = choice_run((const char**)items, n);
	for (unsigned i = 0; i < n; i++)
		string_free(items[i]);
	unsigned result = 0;
	if (r >= 0) {
		menu_get_no(r);
		result = mem_get_sysvar16(mes_sysvar16_menu_no) + 1;
	}
	mem_set_var16(18, result);
}

/*
 * 2D map (Util 1000-1100, HH2D.exe 0x401000-0x409000).
 * The original walks a node graph with pre-rendered views (F00_nnn_dd.map)
 * and returns the room whose door was clicked in var32[12] (Util 1001).
 * For now we offer a room list instead (rooms 1-21, labels from MAPPT.GCC).
 * Right click returns 100 (system menu).
 */
static const struct { unsigned id; const char *name; } map_rooms[] = {
	{  1, "큰 홀" },
	{  2, "홈 바" },
	{  3, "카와다의 방" },
	{  4, "시즈코의 방" },
	{  5, "케이코의 방" },
	{  6, "쿄코의 방" },
	{  7, "미츠코의 방" },
	{  8, "사요코의 방" },
	{  9, "모리타의 방" },
	{ 10, "요시자와의 방" },
	{ 11, "조직원의 방" },
	{ 12, "하자쿠라단의 방" },
	{ 13, "예배당" },
	{ 14, "욕실" },
	{ 15, "현관" },
	{ 16, "주방" },
	{ 17, "지하" },
	{ 18, "조교실" },
	{ 21, "???" },
};

/*
 * Pre-rendered map views: 1024-byte BGRx palette + 640x480 8-bit pixels.
 * Name: F<variant>_<node>_<dir 0-7>.map (B00 for the basement); falls back
 * to variant 00 when the variant has no own view.
 */
static bool map_draw_view(unsigned floor, unsigned variant, unsigned node, unsigned dir, unsigned dst)
{
	char name[32];
	struct archive_data *file = NULL;
	if (floor == 0 && variant) {
		snprintf(name, sizeof(name), "F%02u_%03u_%02u.MAP", variant, node, dir);
		file = asset_data_load(name);
	}
	if (!file) {
		snprintf(name, sizeof(name), "%c00_%03u_%02u.MAP", floor ? 'B' : 'F', node, dir);
		file = asset_data_load(name);
	}
	if (!file)
		return false;
	if (file->size < 1024 + 640 * 480) {
		archive_data_release(file);
		return false;
	}
	struct cg *cg = cg_alloc_direct(640, 480);
	const uint8_t *pal = file->data;
	const uint8_t *px = file->data + 1024;
	for (unsigned i = 0; i < 640 * 480; i++) {
		cg->pixels[i*4+0] = pal[px[i]*4+2];
		cg->pixels[i*4+1] = pal[px[i]*4+1];
		cg->pixels[i*4+2] = pal[px[i]*4+0];
		cg->pixels[i*4+3] = 255;
	}
	archive_data_release(file);
	gfx_draw_cg(dst, cg);
	cg_free(cg);
	return true;
}

static void util_map_select(void)
{
	// show the current view behind the room list
	unsigned dir = mem_get_var16(3);
	if (dir == 1) dir = 3; else if (dir == 3) dir = 1; // see HH2D.exe 0x407f40
	if (map_draw_view(0, mem_get_var4(2097), mem_get_var16(10), (dir & 3) * 2, 0))
		gfx_screen_dirty();

	const char *items[ARRAY_SIZE(map_rooms)];
	for (unsigned i = 0; i < ARRAY_SIZE(map_rooms); i++)
		items[i] = map_rooms[i].name;
	int r = choice_run(items, ARRAY_SIZE(map_rooms));
	mem_set_var32(12, r < 0 ? 100 : map_rooms[r].id);
}

static void hanahebi_util(struct param_list *params)
{
	if (getenv("HH_DEBUG_UTIL")) {
		string str = string_new("");
		for (unsigned i = 0; i < params->nr_params; i++) {
			if (params->params[i].type == MES_PARAM_STRING)
				str = string_concat_fmt(str, "%s\"%s\"", i ? "," : "", params->params[i].str);
			else
				str = string_concat_fmt(str, "%s%u", i ? "," : "", params->params[i].val);
		}
		NOTICE("Util(%s) [%s]", str, mem_mes_name());
		string_free(str);
	}
	if (vm_expr_param(params, 0) == 1001) {
		util_map_select();
		return;
	}
	if (vm_expr_param(params, 0) == 2000) {
		util_choice();
		return;
	}
	if (util_buttons(vm_expr_param(params, 0), params))
		return;
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
	case 5:
		util_system_menu();
		break;
	case 6: case 10: // system menu closed / redraw menu bar
	case 1005: case 1006: // map: pause/resume (TODO)
		break;
	case 8: // quit
		sys_exit(0);
		break;
	case 9: // back to the title screen
		vm_load_mes("main.mes");
		vm_flag_on(FLAG_RETURN);
		break;
	case 12:
		util_save_screen(param_or(params, 1, 2));
		break;
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
	case 8: // copy color and alpha
		gfx_copy(sx, sy, w, h, src_i, dx, dy, dst_i);
		alpha_copy(sx, sy, w, h, src_i, dx, dy, dst_i);
		break;
	case 0xc:
		gfx_copy(sx, sy, w, h, src_i, dx, dy, dst_i);
		break;
	case 9: // copy with transparent color
		gfx_copy_masked(sx, sy, w, h, src_i, dx, dy, dst_i,
				mem_get_sysvar32(mes_sysvar32_mask_color));
		break;
	case 0xb: // fill dst_surface with color param 7
		gfx_fill(sx, sy, w, h, mem_get_sysvar16(mes_sysvar16_dst_surface),
				param_or(params, 7, 0));
		break;
	case 0xa:
	case 0xd: // clear rect on dst_surface
		gfx_fill(sx, sy, w, h, mem_get_sysvar16(mes_sysvar16_dst_surface), 0);
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

/*
 * Choices are defined with defmenu(n) { "text"; } and the addresses of the
 * entries are in System.var32[7] (memory.menu_entry_addresses); the number of
 * entries is in sysvar16[21].
 */

// System.function[14](addr): run the menu entry at addr (draws its text)
static void hanahebi_draw_string(struct param_list *params)
{
	uint32_t addr = param_or(params, 0, 0);
	if (addr >= sizeof(struct memory))
		return;
	struct vm_pointer saved_ip = vm.ip;
	vm.ip.code = memory_raw;
	vm.ip.ptr = addr;
	game->vm.exec();
	vm.ip = saved_ip;
}

// System.function[20](addr): var32[18] = length (bytes) of the menu entry text
static void hanahebi_strlen(struct param_list *params)
{
	uint32_t addr = param_or(params, 0, 0);
	if (addr + 1 >= sizeof(struct memory)) {
		set_result(0);
		return;
	}
	// skip TXT/STR opcode
	char *str = mem_get_cstring(addr + 1);
	set_result(str ? strlen(str) : 0);
}

// System.function[16](i): sysvar16[22] = menu number of entry i
static void hanahebi_menu_get_no(struct param_list *params)
{
	menu_get_no(param_or(params, 0, 0));
}

static void hanahebi_defmenu(void)
{
	vm_stmt_defmenu();
	mem_set_sysvar16(mes_sysvar16_nr_menu_entries, menu_count_entries());
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
	menu_reset();
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

/*
 * System.function[10](mode[, color, fill]): show the back buffer (surface 15)
 * on the screen (HH2D.exe 0x450620). The previous screen is kept in 16.
 *   mode 0: immediately, 1/2: crossfade. If fill is set, the back buffer is
 *   first filled with color.
 */
static void hanahebi_present(struct param_list *params)
{
	unsigned mode = param_or(params, 0, 0);
	if (param_or(params, 2, 0))
		gfx_fill(0, 0, 640, 480, BACK_SURFACE, param_or(params, 1, 0));
	gfx_copy(0, 0, 640, 480, 0, 0, 0, PREV_SURFACE);
	if (mode == 0)
		gfx_copy(0, 0, 640, 480, BACK_SURFACE, 0, 0, 0);
	else
		crossfade_to(BACK_SURFACE);
}

// System.function[9](name): load a GCC image into the destination surface
/*
 * Our surfaces have no alpha channel. HH2D.exe keeps the alpha of 32-bit CGs
 * (e.g. sel.gcc, the choice buttons) and blends when drawing; we approximate
 * this by turning transparent pixels into the mask color (sysvar32[9]), which
 * copy_masked and the button overlay treat as transparent.
 */
static void hanahebi_load_image(struct param_list *params)
{
	const char *name = name_param(params, 0, 3);
	if (getenv("HH_DEBUG_UTIL"))
		NOTICE("load_image(\"%s\") -> %u", name, mem_get_sysvar16(mes_sysvar16_dst_surface));
	struct cg *cg = hh_cg_load(name);
	if (!cg) {
		WARNING("Failed to load CG \"%s\"", name);
		return;
	}
	if (params->nr_params >= 3) {
		cg->metrics.x = param_or(params, 1, cg->metrics.x);
		cg->metrics.y = param_or(params, 2, cg->metrics.y);
	}
	unsigned dst = mem_get_sysvar16(mes_sysvar16_dst_surface);
	if (cg->metrics.bpp == 32) {
		uint32_t mask = mem_get_sysvar32(mes_sysvar32_mask_color);
		uint8_t *a = alpha_get(dst);
		uint8_t *p = cg->pixels;
		for (unsigned y = 0; y < cg->metrics.h; y++) {
			for (unsigned x = 0; x < cg->metrics.w; x++, p += 4) {
				int ax = cg->metrics.x + x, ay = cg->metrics.y + y;
				if (a && ax >= 0 && ax < SURF_W && ay >= 0 && ay < SURF_H)
					a[ay * SURF_W + ax] = p[3];
				if (p[3] == 0) {
					p[0] = (mask >> 16) & 0xff;
					p[1] = (mask >> 8) & 0xff;
					p[2] = mask & 0xff;
				}
				p[3] = 255;
			}
		}
	} else {
		alpha_fill(cg->metrics.x, cg->metrics.y, cg->metrics.w, cg->metrics.h, dst, 255);
	}
	mem_set_sysvar16(mes_sysvar16_cg_x, cg->metrics.x);
	mem_set_sysvar16(mes_sysvar16_cg_y, cg->metrics.y);
	mem_set_sysvar16(mes_sysvar16_cg_w, cg->metrics.w);
	mem_set_sysvar16(mes_sysvar16_cg_h, cg->metrics.h);
	gfx_draw_cg(dst, cg);
	cg_free(cg);
}

static void hanahebi_mem_restore(void);

/*
 * Save data (HH2D.exe, folder SAVEHANAHEBI):
 *   FLAG%03u        mem16 (0x4000 bytes); FLAG000 holds the global variables
 *   SaveData%03u.dat screen records (System.function[24]) at the last save point
 *   MesSkip.dat     "already read" bits (System.function[26])
 *
 * Save points: statement 0x17 n sets sysvar32[8] = n; after loading, the MES
 * file is restarted and (seeing var4[4002] == 1) redraws the screen from the
 * screen records and jumps with statement 0x18 to address table[sysvar32[8]].
 */
#define SAVE_DIR "SAVEHANAHEBI"
#define SV32_SAVE_POINT 8

static const char *save_path(const char *fmt, unsigned no)
{
	static char dir[1024] = "";
	static char path[1100];
	if (!dir[0]) {
		char *d = path_get_icase(SAVE_DIR);
		if (!d) {
			if (mkdir_p(SAVE_DIR))
				WARNING("hanahebi: cannot create " SAVE_DIR);
			d = strdup(SAVE_DIR);
		}
		snprintf(dir, sizeof(dir), "%s", d);
		free(d);
	}
	char name[64];
	snprintf(name, sizeof(name), fmt, no);
	snprintf(path, sizeof(path), "%s/%s", dir, name);
	return path;
}

static const char *flag_file(unsigned no)
{
	return save_path("FLAG%03u", no);
}

static void hh_stmt_save_point(void)
{
	uint32_t n = le_get32(vm.ip.code, vm.ip.ptr);
	vm.ip.ptr += 4;
	mem_set_sysvar32(SV32_SAVE_POINT, n);
	void screen_records_snapshot(void);
	screen_records_snapshot();
}

static void hh_stmt_jump_save_point(void)
{
	uint32_t n = vm_eval();
	if (n >= vm_mes_table_size) {
		WARNING("hanahebi: invalid save point %u", n);
		return;
	}
	vm.ip.ptr = vm_mes_table[n];
}

static void hh_resume_load(unsigned no)
{
	uint8_t *mem = memory_raw;
	uint8_t global[VAR4_OFF + VAR4_SIZE];
	savedata_read(flag_file(no), mem, 0, ARGS_OFF);
	savedata_read(flag_file(0), global, VAR4_OFF, VAR4_SIZE);
	uint8_t *var4 = mem + VAR4_OFF, *gvar4 = global + VAR4_OFF;
	// system variables (var4[3000..]) come from the global file, except these
	uint8_t keep4002 = var4[4002], keep4210 = var4[4210], keep4251 = var4[4251];
	memcpy(var4 + 3000, gvar4 + 3000, VAR4_SIZE - 3000);
	var4[4002] = keep4002;
	var4[4210] = keep4210;
	var4[4251] = keep4251;
	var4[4010] = 0; var4[4003] = 0; var4[4090] = 0; var4[4091] = 0xff;
	var4[4092] = 0; var4[4121] = 0; var4[4122] = 0; var4[4096] = 0;
	var4[4510] = 0; var4[4013] = 1;
	hanahebi_mem_restore();
	uint16_t flags = mem_get_sysvar16(mes_sysvar16_flags);
	mem_set_sysvar16(mes_sysvar16_flags, (flags | 0x8) & 0x7fff);
	vm_load_mes(mem_mes_name());
	vm_flag_on(FLAG_RETURN);
}

// System.function[4](3, slot): merge var4 into a save file
static void hh_save_union_var4(unsigned no)
{
	uint8_t buf[VAR4_OFF + VAR4_SIZE];
	memset(buf, 0, sizeof(buf));
	savedata_read(flag_file(no), buf, 0, sizeof(buf));
	uint8_t *f = buf + VAR4_OFF, *v = memory_raw + VAR4_OFF;
	for (unsigned i = 0; i < VAR4_SIZE; i++) {
		if (i == 4250)
			f[i] = v[i];
		else if (i < 5000)
			f[i] = max(f[i], v[i]);
		else if (v[i])
			f[i] = v[i];
	}
	savedata_write(flag_file(no), buf, VAR4_OFF, VAR4_SIZE);
}

static void hanahebi_savedata(struct param_list *params)
{
	unsigned no = param_or(params, 1, 0);
	unsigned start = param_or(params, 2, 0), count = param_or(params, 3, 0);
	switch (param_or(params, 0, 0)) {
	case 0: hh_resume_load(no); break;
	case 1: savedata_write(flag_file(no), memory_raw, 0, ARGS_OFF); break;
	case 2: // load var4
		savedata_read(flag_file(no), memory_raw, VAR4_OFF, VAR4_SIZE);
		mem_set_var4(4013, 1);
		break;
	case 3: hh_save_union_var4(no); break;
	case 5: // load u16[start..] at 0x2180
	case 6: // save
	case 7: // load bytes at 0x2310 + start
	case 8: { // save
		unsigned base = param_or(params, 0, 0) <= 6 ? 0x2180 : 0x2310;
		unsigned size = param_or(params, 0, 0) <= 6 ? 2 : 1;
		if (base + (start + count) * size > ARGS_OFF) {
			WARNING("hanahebi: invalid save range %u+%u", start, count);
			break;
		}
		if (param_or(params, 0, 0) & 1)
			savedata_read(flag_file(no), memory_raw, base + start * size, count * size);
		else
			savedata_write(flag_file(no), memory_raw, base + start * size, count * size);
		break;
	}
	default:
		log_unimplemented("System", 4, params);
	}
}

/*
 * System.function[24]: screen records (what is on screen, for redrawing after
 * a load). Records are 46 bytes: u16 id, char name[32], u16 p[6]; kept sorted
 * by id. A snapshot is taken at each save point and that is what gets saved.
 */
#define REC_SIZE 46
#define MAX_RECS 30
struct screen_record { uint16_t id; char name[32]; uint16_t p[6]; };
static struct screen_record recs[MAX_RECS], recs_snap[MAX_RECS];
static unsigned nr_recs = 0, nr_recs_snap = 0;

void screen_records_snapshot(void)
{
	memcpy(recs_snap, recs, sizeof(recs));
	nr_recs_snap = nr_recs;
}

static void recs_remove_if(bool (*pred)(struct screen_record*, void*), void *data)
{
	unsigned j = 0;
	for (unsigned i = 0; i < nr_recs; i++) {
		if (!pred(&recs[i], data))
			recs[j++] = recs[i];
	}
	nr_recs = j;
}

static bool rec_id_is(struct screen_record *r, void *d) { return r->id == *(uint16_t*)d; }
static bool rec_id_p0_is(struct screen_record *r, void *d)
{
	return r->id == ((uint16_t*)d)[0] && r->p[0] == ((uint16_t*)d)[1];
}
static const char *rec_cmp_name;
static bool rec_id_name_is(struct screen_record *r, void *d)
{
	return r->id == *(uint16_t*)d && !strncmp(r->name, rec_cmp_name, 31);
}

static void recs_add(struct screen_record *r)
{
	if (nr_recs >= MAX_RECS) {
		WARNING("hanahebi: too many screen records");
		return;
	}
	// insert after records with the same or lower id (stable)
	unsigned i = nr_recs;
	while (i > 0 && recs[i-1].id > r->id) {
		recs[i] = recs[i-1];
		i--;
	}
	recs[i] = *r;
	nr_recs++;
}

static void recs_write(unsigned no)
{
	uint8_t buf[4 + MAX_RECS * REC_SIZE];
	memset(buf, 0, sizeof(buf));
	le_put32(buf, 0, nr_recs_snap);
	for (unsigned i = 0; i < nr_recs_snap; i++) {
		uint8_t *b = buf + 4 + i * REC_SIZE;
		le_put16(b, 0, recs_snap[i].id);
		memcpy(b + 2, recs_snap[i].name, 32);
		for (unsigned k = 0; k < 6; k++)
			le_put16(b, 34 + k * 2, recs_snap[i].p[k]);
	}
	FILE *f = file_open_utf8(save_path("SaveData%03u.dat", no), "wb");
	if (!f) {
		WARNING("hanahebi: cannot write screen records %u", no);
		return;
	}
	fwrite(buf, sizeof(buf), 1, f);
	fclose(f);
}

static void recs_read(unsigned no)
{
	uint8_t buf[4 + MAX_RECS * REC_SIZE];
	memset(buf, 0, sizeof(buf));
	FILE *f = file_open_utf8(save_path("SaveData%03u.dat", no), "rb");
	if (f) {
		if (fread(buf, 1, sizeof(buf), f) < 4)
			memset(buf, 0, 4);
		fclose(f);
	}
	nr_recs = min(le_get32(buf, 0), MAX_RECS);
	for (unsigned i = 0; i < nr_recs; i++) {
		uint8_t *b = buf + 4 + i * REC_SIZE;
		recs[i].id = le_get16(b, 0);
		memcpy(recs[i].name, b + 2, 32);
		recs[i].name[31] = '\0';
		for (unsigned k = 0; k < 6; k++)
			recs[i].p[k] = le_get16(b, 34 + k * 2);
	}
}

static void hanahebi_screen_records(struct param_list *params)
{
	uint16_t key[2];
	switch (param_or(params, 0, 0)) {
	case 0: nr_recs = 0; break;
	case 1:
		key[0] = param_or(params, 1, 0);
		recs_remove_if(rec_id_is, key);
		break;
	case 2: recs_read(param_or(params, 1, 0)); break;
	case 3: recs_write(param_or(params, 1, 0)); break;
	case 4: {
		struct screen_record r = { .id = param_or(params, 1, 0) };
		const char *name = "";
		if (params->nr_params > 2 && params->params[2].type == MES_PARAM_STRING)
			name = params->params[2].str;
		strncpy(r.name, name, 31);
		for (unsigned k = 0; k < 6; k++)
			r.p[k] = param_or(params, 3 + k, 0);
		recs_add(&r);
		break;
	}
	case 5: {
		unsigned i = param_or(params, 1, 0);
		mem_set_var16(18, i < nr_recs ? recs[i].id : 0xffff);
		break;
	}
	case 6: {
		unsigned i = param_or(params, 1, 0);
		uint32_t dst = param_or(params, 3, 0);
		if (i < nr_recs && dst < sizeof(struct memory) - 32)
			memcpy(memory_raw + dst, recs[i].name, 32);
		break;
	}
	case 7: {
		unsigned i = param_or(params, 1, 0), k = param_or(params, 2, 0);
		if (i < nr_recs && k < 6)
			mem_set_var16(18, recs[i].p[k]);
		break;
	}
	case 8:
		key[0] = param_or(params, 1, 0);
		key[1] = param_or(params, 2, 0);
		recs_remove_if(rec_id_p0_is, key);
		break;
	case 9:
		key[0] = param_or(params, 1, 0);
		rec_cmp_name = name_param(params, 2, 99);
		recs_remove_if(rec_id_name_is, key);
		break;
	default:
		log_unimplemented("System", 24, params);
	}
}

/*
 * System.function[26]: "already read" bits (MesSkip.dat, 0x14f2 bytes).
 *   0 load, 1 save, 2 clear, 3 set bit n, 4 test bit n -> var16[18]
 */
#define MESSKIP_SIZE 0x14f2
static uint8_t mes_skip[MESSKIP_SIZE];

static void hanahebi_read_flags(struct param_list *params)
{
	unsigned n = param_or(params, 1, 0);
	switch (param_or(params, 0, 0)) {
	case 0: {
		FILE *f = file_open_utf8(save_path("MesSkip.dat", 0), "rb");
		if (f) {
			if (fread(mes_skip, 1, MESSKIP_SIZE, f) != MESSKIP_SIZE)
				WARNING("hanahebi: short MesSkip.dat");
			fclose(f);
		}
		break;
	}
	case 1: {
		FILE *f = file_open_utf8(save_path("MesSkip.dat", 0), "wb");
		if (f) {
			fwrite(mes_skip, MESSKIP_SIZE, 1, f);
			fclose(f);
		}
		break;
	}
	case 2: memset(mes_skip, 0, MESSKIP_SIZE); break;
	case 3:
		if (n / 8 < MESSKIP_SIZE)
			mes_skip[n / 8] |= 0x80 >> (n % 8);
		break;
	case 4:
		mem_set_var16(18, n / 8 < MESSKIP_SIZE && (mes_skip[n / 8] & (0x80 >> (n % 8))));
		break;
	default:
		log_unimplemented("System", 26, params);
	}
}

/*
 * Util 12: save/load screen (HH2D.exe draws its own; this one is ours).
 *   (12, 0) save mode, (12, 1) load mode -> var16[18] = 0 save / 1 load / 2 close,
 *   var16[13] = slot (1..32). (12, 2) close, (12, 3, n) nothing.
 * The thumbnail (CG%03u.dat, 96x72 BGR bottom-up) is written when saving.
 */
#define SAVE_SLOTS 32
#define SLOTS_PER_PAGE 8
#define THUMB_W 96
#define THUMB_H 72
static uint8_t thumb_now[THUMB_W * THUMB_H * 3];
static int save_page = 0;

static void thumb_capture(void)
{
	SDL_Surface *src = SDL_ConvertSurfaceFormat(gfx_get_surface(0), SDL_PIXELFORMAT_RGB24, 0);
	if (!src)
		return;
	const int sx = 640 / THUMB_W, sy = 480 / THUMB_H;
	for (int y = 0; y < THUMB_H; y++) {
		for (int x = 0; x < THUMB_W; x++) {
			unsigned r = 0, g = 0, b = 0, n = 0;
			for (int yy = y * 480 / THUMB_H; yy < y * 480 / THUMB_H + sy; yy++) {
				uint8_t *p = (uint8_t*)src->pixels + yy * src->pitch + (x * 640 / THUMB_W) * 3;
				for (int xx = 0; xx < sx; xx++, p += 3, n++) {
					r += p[0]; g += p[1]; b += p[2];
				}
			}
			// stored bottom-up BGR like a DIB
			uint8_t *d = thumb_now + ((THUMB_H - 1 - y) * THUMB_W + x) * 3;
			d[0] = b / n; d[1] = g / n; d[2] = r / n;
		}
	}
	SDL_FreeSurface(src);
}

struct slot_info { bool used; char date[32]; SDL_Surface *thumb; };

static void slot_info_get(unsigned no, struct slot_info *info)
{
	memset(info, 0, sizeof(*info));
	char *path = path_get_icase(flag_file(no));
	if (!path)
		return;
	FILE *f = file_open_utf8(path, "rb");
	char name[16] = {0};
	if (f) {
		if (fread(name, 1, 15, f) != 15)
			name[0] = 0;
		fclose(f);
	}
	info->used = name[0] && strncasecmp(name, "FLAGINI", 7);
	struct stat st;
	if (info->used && !stat(path, &st)) {
		struct tm *tm = localtime(&st.st_mtime);
		strftime(info->date, sizeof(info->date), "%Y/%m/%d %H:%M", tm);
	}
	free(path);
	if (!info->used)
		return;
	f = file_open_utf8(save_path("CG%03u.dat", no), "rb");
	if (!f)
		return;
	uint8_t buf[THUMB_W * THUMB_H * 3];
	if (fread(buf, sizeof(buf), 1, f) == 1) {
		info->thumb = SDL_CreateRGBSurfaceWithFormat(0, THUMB_W, THUMB_H, 24, SDL_PIXELFORMAT_RGB24);
		for (int y = 0; y < THUMB_H; y++) {
			uint8_t *d = (uint8_t*)info->thumb->pixels + y * info->thumb->pitch;
			uint8_t *s = buf + (THUMB_H - 1 - y) * THUMB_W * 3;
			for (int x = 0; x < THUMB_W; x++, d += 3, s += 3) {
				d[0] = s[2]; d[1] = s[1]; d[2] = s[0];
			}
		}
	}
	fclose(f);
}

static void work_text(int x, int y, const char *text)
{
	while (*text) {
		int ch;
		text = utf8_next(text, &ch);
		x += gfx_text_draw_glyph(x, y, WORK_SURFACE, ch);
	}
}

// screen layout
enum { SL_TAB_SAVE = 100, SL_TAB_LOAD, SL_CLOSE, SL_PAGE0 = 200 };
static void slot_rect(int id, SDL_Rect *r)
{
	if (id >= SL_PAGE0) {
		*r = (SDL_Rect){ 200 + (id - SL_PAGE0) * 60, 436, 52, 34 };
	} else if (id == SL_TAB_SAVE) {
		*r = (SDL_Rect){ 20, 10, 130, 40 };
	} else if (id == SL_TAB_LOAD) {
		*r = (SDL_Rect){ 160, 10, 130, 40 };
	} else if (id == SL_CLOSE) {
		*r = (SDL_Rect){ 510, 10, 110, 40 };
	} else {
		*r = (SDL_Rect){ 20 + (id % 2) * 306, 60 + (id / 2) * 92, 294, 86 };
	}
}

static int slot_hit(int x, int y)
{
	static const int ids[] = { SL_TAB_SAVE, SL_TAB_LOAD, SL_CLOSE,
		SL_PAGE0, SL_PAGE0 + 1, SL_PAGE0 + 2, SL_PAGE0 + 3 };
	SDL_Rect r;
	for (int i = 0; i < SLOTS_PER_PAGE; i++) {
		slot_rect(i, &r);
		if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
			return i;
	}
	for (unsigned i = 0; i < ARRAY_SIZE(ids); i++) {
		slot_rect(ids[i], &r);
		if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
			return ids[i];
	}
	return -1;
}

static void box(SDL_Surface *ov, int id, bool hl, bool active)
{
	SDL_Rect r, in;
	slot_rect(id, &r);
	in = (SDL_Rect){ r.x + 2, r.y + 2, r.w - 4, r.h - 4 };
	SDL_CALL(SDL_FillRect, ov, &r, SDL_MapRGBA(ov->format, hl ? 230 : 150, 40, 40, 240));
	SDL_CALL(SDL_FillRect, ov, &in, SDL_MapRGBA(ov->format,
			active ? 110 : hl ? 70 : 25, active ? 30 : 20, active ? 30 : 20, 235));
}

static void save_screen_draw(bool save_mode, struct slot_info *info, int hover)
{
	SDL_Surface *ov = gfx_get_overlay(2);
	SDL_CALL(SDL_FillRect, ov, NULL, SDL_MapRGBA(ov->format, 0, 0, 0, 200));
	gfx_fill(0, 0, 640, 480, WORK_SURFACE, 0);
	gfx_text_set_colors(0, 0xffffff);
	gfx_text_set_size(22, 0);
	box(ov, SL_TAB_SAVE, hover == SL_TAB_SAVE, save_mode);
	box(ov, SL_TAB_LOAD, hover == SL_TAB_LOAD, !save_mode);
	box(ov, SL_CLOSE, hover == SL_CLOSE, false);
	work_text(20 + 43, 19, "저장");
	work_text(160 + 21, 19, "불러오기");
	work_text(510 + 33, 19, "닫기");
	for (int i = 0; i < 4; i++) {
		box(ov, SL_PAGE0 + i, hover == SL_PAGE0 + i, save_page == i);
		char t[4];
		snprintf(t, sizeof(t), "%d", i + 1);
		work_text(200 + i * 60 + 20, 442, t);
	}
	gfx_text_set_size(18, 0);
	for (int i = 0; i < SLOTS_PER_PAGE; i++) {
		SDL_Rect r;
		slot_rect(i, &r);
		box(ov, i, hover == i, false);
		struct slot_info *si = &info[i];
		SDL_Rect tr = { r.x + 7, r.y + 7, THUMB_W, THUMB_H };
		if (si->thumb) {
			SDL_CALL(SDL_BlitSurface, si->thumb, NULL, ov, &tr);
		} else {
			SDL_CALL(SDL_FillRect, ov, &tr, SDL_MapRGBA(ov->format, 0, 0, 0, 255));
		}
		char t[48];
		snprintf(t, sizeof(t), "No.%02d", save_page * SLOTS_PER_PAGE + i + 1);
		work_text(r.x + 115, r.y + 12, t);
		work_text(r.x + 115, r.y + 48, si->used ? si->date : "비어 있음");
	}
	SDL_Surface *ts = gfx_get_surface(WORK_SURFACE);
	SDL_CALL(SDL_SetColorKey, ts, SDL_TRUE, SDL_MapRGB(ts->format, 0, 0, 0));
	SDL_CALL(SDL_BlitSurface, ts, NULL, ov, NULL);
	SDL_CALL(SDL_SetColorKey, ts, SDL_FALSE, 0);
	gfx_overlay_enable(2);
	gfx_screen_dirty();
}

static bool confirm(const char *question)
{
	const char *items[] = { question, "예", "아니오" };
	// the first item is only a label; pick again if chosen
	while (true) {
		int r = choice_run(items, 3);
		if (r != 0)
			return r == 1;
	}
}

/*
 * Util 5: system menu. HH2D.exe uses the Windows menu bar; we show a popup.
 * var16[18]: 1 save, 2 load, 4 backlog, 5 hide message window, 7 status,
 * 9 title, 10 quit, 11 (or cancel) close.
 */
static void util_system_menu(void)
{
	static const struct { const char *text; unsigned code; } items[] = {
		{ "저장하기", 1 },
		{ "불러오기", 2 },
		{ "지난 대사 보기", 4 },
		{ "메시지창 숨기기", 5 },
		{ "타이틀로 돌아가기", 9 },
		{ "게임 종료", 10 },
		{ "돌아가기", 11 },
	};
	const char *text[ARRAY_SIZE(items)];
	for (unsigned i = 0; i < ARRAY_SIZE(items); i++)
		text[i] = items[i].text;
	int r = choice_run(text, ARRAY_SIZE(items));
	unsigned code = r < 0 ? 11 : items[r].code;
	if ((code == 9 || code == 10) && !confirm(code == 9 ? "타이틀로 돌아갈까요?" : "게임을 끝낼까요?"))
		code = 11;
	mem_set_var16(18, code);
}

static void util_save_screen(unsigned mode)
{
	if (mode >= 2)
		return;
	bool save_mode = mode == 0;
	thumb_capture();
	struct slot_info info[SLOTS_PER_PAGE];
	bool reload = true;
	int hover = -1, last = -2, action = 2, slot = 0;
	input_wait_until_up(INPUT_ACTIVATE);
	input_wait_until_up(INPUT_CANCEL);
	while (true) {
		if (reload) {
			for (int i = 0; i < SLOTS_PER_PAGE; i++)
				slot_info_get(save_page * SLOTS_PER_PAGE + i + 1, &info[i]);
			reload = false;
			last = -2;
		}
		unsigned cx, cy;
		cursor_get_pos(&cx, &cy);
		hover = slot_hit(cx, cy);
		if (hover != last) {
			save_screen_draw(save_mode, info, hover);
			last = hover;
		}
		if (input_down(INPUT_CANCEL)) {
			input_wait_until_up(INPUT_CANCEL);
			break;
		}
		if (input_down(INPUT_ACTIVATE)) {
			cursor_get_pos(&cx, &cy);
			hover = slot_hit(cx, cy);
		}
		if (!input_down(INPUT_ACTIVATE) || hover < 0) {
			vm_peek();
			vm_delay(16);
			continue;
		}
		input_wait_until_up(INPUT_ACTIVATE);
		if (hover == SL_CLOSE)
			break;
		if (hover == SL_TAB_SAVE || hover == SL_TAB_LOAD) {
			save_mode = hover == SL_TAB_SAVE;
			last = -2;
			continue;
		}
		if (hover >= SL_PAGE0) {
			save_page = hover - SL_PAGE0;
			reload = true;
			continue;
		}
		int no = save_page * SLOTS_PER_PAGE + hover + 1;
		if (!save_mode && !info[hover].used)
			continue;
		bool ok = save_mode
			? (!info[hover].used || confirm("이 칸에 덮어쓸까요?"))
			: confirm("이 데이터를 불러올까요?");
		last = -2;
		if (!ok)
			continue;
		action = save_mode ? 0 : 1;
		slot = no;
		break;
	}
	for (int i = 0; i < SLOTS_PER_PAGE; i++) {
		if (info[i].thumb)
			SDL_FreeSurface(info[i].thumb);
	}
	if (action == 0) {
		FILE *f = file_open_utf8(save_path("CG%03u.dat", slot), "wb");
		if (f) {
			fwrite(thumb_now, sizeof(thumb_now), 1, f);
			fclose(f);
		}
	}
	gfx_overlay_disable(2);
	gfx_screen_dirty();
	mem_set_var16(18, action);
	mem_set_var16(13, slot);
}

static void hanahebi_mem_restore(void)
{
	mem_set_sysvar16_ptr(SYSVAR16_OFF);
	mem_set_sysvar32(mes_sysvar32_memory, offsetof(struct memory, mem16));
	// sysvar32[8] is the current save point (statement 0x17), not file_data
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

	// HH2D.exe (2D version) initialization (0x40e600):
	//   var4[4013] = 1 (2D mode), sysvar16[23] = 15 (back buffer surface),
	//   flags |= 0x402 | 0x2000 (0x2000 unless [DispEffect] Play == 3)
	mem_set_var4(4013, 1);
	mem_set_sysvar16(23, BACK_SURFACE);
	uint16_t flags = mem_get_sysvar16(mes_sysvar16_flags) | 0x402 | 0x2000;
	const char *play = config_ini_get("DispEffect", "Play");
	if (play && atoi(play) == 3)
		flags &= ~0x2000;
	mem_set_sysvar16(mes_sysvar16_flags, flags);
}

static void hanahebi_init(void)
{
	// sound effects are in their own archive (se.arc)
	asset_effect_is_bgm = false;
	// text is drawn on a black (transparent) background and shown over a
	// dark box, so antialiased edges look fine
	text_antialias = true;
}

struct game game_hanahebi = {
	.id = GAME_HANAHEBI,
	// 0 = screen, 15 = back buffer, 16 = previous screen (HH2D.exe);
	// 17, 18 = our own work surfaces
	.surface_sizes = {
		[0 ... 18] = { 640, 480 },
		[19] = { 0, 0 },
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
		[0x10] = hanahebi_defmenu,
		[0x11] = vm_stmt_call,
		[0x12] = hanahebi_stmt_util,
		[0x13] = vm_stmt_line,
		[0x14] = vm_stmt_defproc,
		[0x15] = hanahebi_menuexec,
		[0x16] = vm_stmt_set_var32_const8,
		[0x17] = hh_stmt_save_point,
		[0x18] = hh_stmt_jump_save_point,
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
		[4]  = hanahebi_savedata,
		[5]  = hanahebi_bgm,
		[6]  = hanahebi_se,
		[7]  = hanahebi_voice,
		[8]  = hanahebi_load,
		[9]  = hanahebi_load_image,
		[10] = hanahebi_present,
		[11] = hanahebi_graphics,
		[12] = hanahebi_wait,
		[13] = hanahebi_set_colors,
		[14] = hanahebi_draw_string,
		[16] = hanahebi_menu_get_no,
		[18] = hanahebi_check_input,
		[19] = hanahebi_backlog,
		[20] = hanahebi_strlen,
		[23] = sys_stub_23,
		[24] = hanahebi_screen_records,
		[26] = hanahebi_read_flags,
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
		[FLAG_WAIT_KEYUP] = FLAG_ALWAYS_ON,
	},
};
