/* Copyright (C) 2026 ai5-sdl2 fork contributors
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

/*
 * Training mini games of Hana to Hebi (HH2D.exe), included by hanahebi.c.
 *
 * Util 66 (HH2D.exe 0x42ea30..0x431500): "rope" timing game, mini01.gcc.
 *   A pink icon walks along a rope (1 px every 17 ms, x 8..508) with knots
 *   from one of 20 patterns (0x55fdd0, 48 cells: 1 = knot start, 3 = long
 *   knot, 2 = knot end). Press when the icon reaches a knot and let go at
 *   its end: error = |press - start| + |release - end| < 8 great (+2),
 *   < 16 good (+1), else bad (-1); a knot passed without pressing is bad.
 *   The first round waits for [開始]; [中断] aborts (var32[2] = 1).
 *     (66, 0, chr, tool, x, y)  set up; x, y = the zoom window (260x208)
 *     (66, 3, n)                frames of the zoom animation
 *     (66, 4, "voice", kind)    voice for a round grade (0..3)
 *     (66, 5, "x.itm")          10 bytes: zoom animation jumps
 *     (66, 1, pattern)          one round (the scripts play 3)
 *     (66, 6)                   System.var16[200] = score (0..7)
 *     (66, 7, x, y)             automatic: zoom animation and voices only
 *     (66, 2)                   end
 *
 * Util 67 (HH2D.exe 0x45de20..0x460350): "target" game, target.gcc and
 *   eff_lash/bamb/cand.gcc. A target appears on the picture; its ring
 *   shrinks every 75 ms (10 steps, twice). Click it: great (+2) when the
 *   ring step is in the table at 0x597cb0, else good (+1); clicking the
 *   body elsewhere (the .dat region map) is a miss (-1), letting the ring
 *   run out too. Each result shows an effect, a reaction face (surfaces
 *   2..5, green = transparent) and a voice.
 *     (67, 0, chr, tool, x, y, level, corner)  tool 5..7 = lash, bamboo, candle
 *     (67, 4, "voice", kind, target)  kind 0 time out, 1 good, 2 great, 3 miss
 *     (67, 5, "x.itm")          10 target positions (u32 x, y)
 *     (67, 6, "x.dat")          640x480 region map (rows bottom-up)
 *     (67, 3, n, kind)          next target: position n, kind 0..2
 *     (67, 1)                   one round (the scripts play 5)
 *     (67, 7)                   System.var16[200] = score (0..7)
 *     (67, 8, tool)             automatic demonstration
 *     (67, 2)                   end
 *
 * Score (0x42e6e0 / 0x45d170): rate > 89% 7, > 79 5, > 69 3, > 49 2, > 29 1.
 */

#define MG_OVERLAY 2

static int mg2_region_cursor(void);

static void mg_score_result(int got, int max)
{
	int pct = max > 0 ? got * 100 / max : 0;
	unsigned v = pct > 89 ? 7 : pct > 79 ? 5 : pct > 69 ? 3 : pct > 49 ? 2 : pct > 29 ? 1 : 0;
	mem_set_sysvar16(200, v);
	NOTICE("mini game: %d / %d -> %u", got, max, v);
}

/*
 * A sheet: the CG at its own position (so source rectangles match the
 * original), as RGBA. Green (the mask color of these sheets) is transparent.
 */
static SDL_Surface *mg_sheet(const char *name)
{
	struct cg *cg = hh_cg_load(name);
	if (!cg)
		return NULL;
	if (cg->palette || cg->metrics.bpp == 8) {
		cg_free(cg);
		return NULL;
	}
	int ox = cg->metrics.x, oy = cg->metrics.y;
	SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, ox + cg->metrics.w, oy + cg->metrics.h,
			32, SDL_PIXELFORMAT_RGBA32);
	SDL_FillRect(s, NULL, 0);
	for (unsigned y = 0; y < cg->metrics.h; y++) {
		uint8_t *src = cg->pixels + y * cg->metrics.w * 4;
		uint8_t *dst = (uint8_t*)s->pixels + (y + oy) * s->pitch + ox * 4;
		for (unsigned x = 0; x < cg->metrics.w; x++, src += 4, dst += 4) {
			bool key = !src[0] && src[1] == 255 && !src[2];
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = key ? 0 : src[3];
		}
	}
	SDL_SetSurfaceBlendMode(s, SDL_BLENDMODE_BLEND);
	cg_free(cg);
	return s;
}

static void mg_blit(SDL_Surface *src, int sx, int sy, int w, int h, int dx, int dy)
{
	if (!src)
		return;
	SDL_Rect r = { sx, sy, w, h }, d = { dx, dy, w, h };
	SDL_BlitSurface(src, &r, gfx_get_overlay(MG_OVERLAY), &d);
}

static void mg_clear(void)
{
	SDL_Surface *ov = gfx_get_overlay(MG_OVERLAY);
	SDL_FillRect(ov, NULL, SDL_MapRGBA(ov->format, 0, 0, 0, 0));
}

static void mg_show(void)
{
	gfx_overlay_enable(MG_OVERLAY);
	gfx_screen_dirty();
}

static void mg_hide(void)
{
	mg_clear();
	gfx_overlay_disable(MG_OVERLAY);
	gfx_screen_dirty();
}

// [開始] / [中断] buttons (112x48); Korean mode: Korean text on them
static void mg_button(SDL_Surface *sheet, int sx, int sy, int dx, int dy, bool abort_label)
{
	mg_blit(sheet, sx, sy, 112, 48, dx, dy);
	if (!text_encoding_is_cp949())
		return;
	SDL_Surface *ov = gfx_get_overlay(MG_OVERLAY);
	ov_erase(ov, dx + 26, dy + 12, dx + 86, dy + 36, dx + 24, dy + 24);
	ov_text(ov, dx + 56, dy + 13, 20, abort_label ? "중  단" : "시  작", 0xffffff, NULL);
}

struct mg_voice {
	char name[32];
	uint8_t kind, filter;
};
#define MG_MAX_VOICES 64
static struct mg_voice mg_voices[MG_MAX_VOICES];
static unsigned mg_nr_voices;

static void mg_voice_add(const char *name, unsigned kind, unsigned filter)
{
	if (mg_nr_voices >= MG_MAX_VOICES)
		return;
	struct mg_voice *v = &mg_voices[mg_nr_voices++];
	snprintf(v->name, sizeof(v->name), "%s", name);
	v->kind = kind;
	v->filter = filter;
}

// a random voice of the kind (filter: 0xff = any, else the target number)
static bool mg_voice_play(unsigned kind, int target)
{
	if (!setting_enabled("Voice", "IsVoice"))
		return false;
	unsigned idx[MG_MAX_VOICES], n = 0;
	for (unsigned i = 0; i < mg_nr_voices; i++) {
		if (mg_voices[i].kind != kind)
			continue;
		if (target >= 0 && mg_voices[i].filter != 0xff && mg_voices[i].filter != target)
			continue;
		idx[n++] = i;
	}
	if (!n)
		return false;
	audio_voice_play(mg_voices[idx[rand() % n]].name, 0);
	apply_volume(AUDIO_CH_VOICE0, "Voice");
	return true;
}

static void mg_se(const char *name)
{
	if (!name || !*name || !setting_enabled("Se", "IsSe"))
		return;
	audio_se_play(name, 1);
	apply_volume(AUDIO_CH_SE(1), "Effect");
}

// input edges for the mini games
static bool mg_prev_down;

static void mg_input_reset(void)
{
	mg_prev_down = input_down(INPUT_ACTIVATE);
}

// 1 = pressed now, -1 = released now, 0 = no change
static int mg_input_edge(void)
{
	bool d = input_down(INPUT_ACTIVATE);
	int e = d == mg_prev_down ? 0 : d ? 1 : -1;
	mg_prev_down = d;
	return e;
}

static void mg_frame(void);

// after [開始]: let go of the button first (the same click must not abort)
static void mg_wait_release(void)
{
	while (input_down(INPUT_ACTIVATE))
		mg_frame();
	mg_input_reset();
}

// test switch HH_DEBUG_MINI=1: play the mini games perfectly
static bool mg_debug_perfect(void)
{
	const char *v = getenv("HH_DEBUG_MINI");
	return v && *v == '1';
}

static bool mg_skip_keys(void)
{
	return input_down(INPUT_CTRL) || input_down(INPUT_SHIFT);
}

static void mg_frame(void)
{
	mg_show();
	vm_peek();
	vm_delay(8);
}

/* ------------------------------------------------------------------ Util 66 */

static const char *const mg1_patterns[20] = {
	"......12..12..13332..12..12..13332..12..1213332.",
	"......12.13332..12.12.13332..12.12.13332..12.12.",
	"......13332....13332....13332....13332....13332.",
	"......133332.....133332.....133332.....133332...",
	"....13332.12.12.12.12.12....13332.12.12.12.12.12",
	"....1332.12.12.1332.12......1332.12.12.1332.12..",
	"....12....12....12....12....12....12....12....12",
	"......12.1332...12.1332...12.1332...12.1332.....",
	"....12.12.12..1333332.......12.12.12..1333332...",
	"....12.12...12.133332.......12.12...12.133332...",
	"......12..12..12..12..12......12..12..12..12..12",
	"....12.12.12.12.12...12.....12.12.12.12.12...12.",
	"....12.12.12.12.12.12.......12.12.12.12.12.12...",
	"....12.12...12.12...1332....12.12...12.12...1332",
	".....1332.1332.1332.1332.....1332.1332.1332.1332",
	"....13332.12...13332.12.....13332.12...13332.12.",
	"......12.12.1332.12.12.....12.12.1332.12.12.....",
	"......12...12...1212...12...12...12...1212...12.",
	"......12.12....12.12....12.12....12.12....12.12.",
	"....12.12.12.12.133332......12.12.12.12.133332..",
};

#define MG1_STEP_MS  17   // 0x4309a1: the icon moves 1 px every 17 ms
#define MG1_END_X    0x1bc
#define MG1_STOP_X   0x1fc
#define MG1_BAR_Y    408  // the rope layer
#define MG1_BTN_X    528
#define MG1_BTN_Y    432

struct mg1_note { int start, end; bool done; };
struct mg1_mark { int x, kind, frame; uint32_t t; };

static struct {
	SDL_Surface *sheet;
	int chr, tool, zx, zy;
	int anim_frames;
	uint8_t itm[10];
	bool started;
	int score, total;    // 0x124 / 0x128
	// round
	struct mg1_note notes[48];
	int nr_notes;
	int x;               // 0xe0
	bool holding;        // 0xf0
	int press_x;
	int round_score;     // 0xfc
	int misses;          // 0xec
	int icon_state, icon_frame; // 0x90, 0x94
	uint32_t icon_t, step_t;
	struct mg1_mark marks[16];
	int nr_marks;
	int grade_kind, grade_frame; // big 甲乙丙 after a round
	uint32_t grade_t;
	int zoom_frame;
	uint32_t zoom_t;
	bool loop_se;
	const char *loop_name;
} mg1;

static bool mg_zoom_enabled(void)
{
	const char *v = config_ini_get("OPTION", "Train");
	return !v || atoi(v) != 0;
}

// zoom window: frames of the tall picture kept from surface 1 (pano)
static void mg1_draw_zoom(int zx, int zy)
{
	if (!pano || !mg_zoom_enabled())
		return;
	int f = mg1.zoom_frame;
	SDL_Rect src = { (f / 7) * 260, (f % 7) * 208, 260, 208 };
	SDL_Rect dst = { zx, zy, 260, 208 };
	SDL_Surface *ov = gfx_get_overlay(MG_OVERLAY);
	// pano is in the screen format: blit opaque
	SDL_SetSurfaceBlendMode(pano, SDL_BLENDMODE_NONE);
	SDL_BlitSurface(pano, &src, ov, &dst);
	// white frame
	uint32_t white = SDL_MapRGBA(ov->format, 255, 255, 255, 255);
	SDL_Rect t = { zx - 2, zy - 2, 264, 2 }, b = { zx - 2, zy + 208, 264, 2 };
	SDL_Rect l = { zx - 2, zy, 2, 208 }, r = { zx + 260, zy, 2, 208 };
	SDL_FillRect(ov, &t, white);
	SDL_FillRect(ov, &b, white);
	SDL_FillRect(ov, &l, white);
	SDL_FillRect(ov, &r, white);
}

// 0x42efb0: zoom animation (faster while the button is held)
static void mg1_zoom_tick(void)
{
	int n = mg1.anim_frames > 0 ? mg1.anim_frames : 1;
	uint32_t now = SDL_GetTicks();
	if (now < mg1.zoom_t + (mg1.holding ? 0x1c : 0x52))
		return;
	mg1.zoom_t = now;
	mg1.zoom_frame++;
	if (mg1.zoom_frame >= n)
		mg1.zoom_frame = 0;
	if (!mg1.holding && mg1.zoom_frame < 10 && mg1.itm[mg1.zoom_frame] == 0xff)
		mg1.zoom_frame = 0;
}

static void mg1_zoom_press(void)
{
	if (mg1.zoom_frame < 10 && mg1.itm[mg1.zoom_frame] != 0xff)
		mg1.zoom_frame = mg1.itm[mg1.zoom_frame];
}

static void mg1_zoom_release(void)
{
	for (int i = 0; i < 10; i++) {
		if (mg1.zoom_frame <= mg1.itm[i]) {
			mg1.zoom_frame = i;
			return;
		}
	}
}

static void mg1_add_mark(int x, int kind)
{
	if (mg1.nr_marks >= 16)
		memmove(mg1.marks, mg1.marks + 1, sizeof(mg1.marks[0]) * --mg1.nr_marks);
	mg1.marks[mg1.nr_marks++] = (struct mg1_mark) { x, kind, 0, SDL_GetTicks() };
}

static const char *mg1_start_sound(void)
{
	static const char *tool_se[3] = { "hh_134.wav", "hh_094.wav", "hh_132.wav" };
	static const char *anal_se[5] = { "hh_100a.wav", "hh_100b.wav", "hh_100c.wav",
		"hh_100d.wav", "hh_100e.wav" };
	if (mg1.tool >= 0 && mg1.tool < 3)
		return tool_se[mg1.tool];
	if (mg1.tool == 4 && mg1.chr >= 0 && mg1.chr < 5)
		return anal_se[mg1.chr];
	return NULL;
}

static void mg1_loop_se_start(void)
{
	mg1.loop_name = mg1_start_sound();
	mg1.loop_se = mg1.loop_name != NULL;
	if (mg1.loop_se)
		mg_se(mg1.loop_name);
}

static void mg1_loop_se_stop(void)
{
	if (mg1.loop_se)
		audio_se_stop(1);
	mg1.loop_se = false;
}

static void mg1_draw(bool button_hover)
{
	mg_clear();
	SDL_Surface *s = mg1.sheet;
	mg1_draw_zoom(mg1.zx, mg1.zy);
	// rope with knots and the two end marks
	mg_blit(s, 0, 0, 640, 24, 0, MG1_BAR_Y + 0x24);
	mg_blit(s, 0x30, 0x18, 12, 12, 10, MG1_BAR_Y + 0x2a);
	mg_blit(s, 0x30, 0x18, 12, 12, 0x1c2, MG1_BAR_Y + 0x2a);
	for (int i = 0; i < mg1.nr_notes; i++) {
		struct mg1_note *n = &mg1.notes[i];
		for (int x = n->start - 7; x <= n->end; x += 8) {
			int sx = x == n->start - 7 ? 0x18 : x >= n->end - 1 ? 0x28 : 0x20;
			mg_blit(s, sx, 0x18, 8, 16, x - 1, MG1_BAR_Y + 0x26);
		}
	}
	// the icon: 0 normal, 1 on a knot, 2 held
	int ix = mg1.x;
	if (mg1.icon_state == 2)
		mg_blit(s, 0xac + mg1.icon_frame * 0x18, 0x50, 0x18, 0x24, ix - 4, MG1_BAR_Y - 4);
	else
		mg_blit(s, 0xac + mg1.icon_frame * 0x10, mg1.icon_state ? 0x34 : 0x18, 0x10, 0x1c,
				ix, MG1_BAR_Y);
	// judgement marks (40x60, 10 frames)
	for (int i = 0; i < mg1.nr_marks; i++) {
		struct mg1_mark *m = &mg1.marks[i];
		mg_blit(s, m->frame * 0x28, (m->kind + 2) * 0x3c, 0x28, 0x3c, m->x, 0x19c);
	}
	// grade of the round (80x144, 10 frames)
	if (mg1.grade_kind >= 0)
		mg_blit(s, mg1.grade_frame * 0x50, mg1.grade_kind * 0x90 + 300, 0x50, 0x90,
				0x1bc, 0x150);
	// [開始] / [中断]
	int state = button_hover ? (input_down(INPUT_ACTIVATE) ? 2 : 1) : 0;
	mg_button(s, 0x10c + state * 0x70, mg1.started ? 0x48 : 0x18, MG1_BTN_X, MG1_BTN_Y,
			mg1.started);
}

static bool mg1_on_button(void)
{
	unsigned cx, cy;
	cursor_get_pos(&cx, &cy);
	return cx >= MG1_BTN_X && cx < MG1_BTN_X + 108 && cy >= MG1_BTN_Y && cy < 480;
}

// 0x42f0b0: the knot nearest to the press
static void mg1_release(int release_x)
{
	mg1.holding = false;
	mg1_zoom_release();
	if (!mg1.nr_notes)
		return;
	int best = 0, bd = INT_MAX;
	for (int i = 0; i < mg1.nr_notes; i++) {
		int d = abs(mg1.press_x - mg1.notes[i].start);
		if (d < bd) {
			bd = d;
			best = i;
		}
	}
	struct mg1_note *n = &mg1.notes[best];
	int kind;
	if (n->done) {
		mg1.misses++;
		kind = 2;
		mg1.round_score--;
	} else {
		int err = abs(mg1.press_x - n->start) + abs(release_x - n->end);
		if (err < 8) {
			n->done = true;
			kind = 0;
			mg1.round_score += 2;
		} else if (err < 16) {
			n->done = true;
			kind = 1;
			mg1.round_score += 1;
		} else {
			mg1.misses++;
			kind = 2;
			mg1.round_score--;
		}
	}
	mg1_add_mark(mg1.x - 0x14, kind);
}

// 0x430df0: knots passed without pressing; returns true on a knot
static bool mg1_check_notes(void)
{
	int c = mg1.x + 8;
	for (int i = 0; i < mg1.nr_notes; i++) {
		struct mg1_note *n = &mg1.notes[i];
		if (n->start - 16 <= c && c <= n->end + 16)
			return true;
		if (n->end + 16 <= c && !n->done) {
			mg1_add_mark(n->end - 0x14, 2);
			n->done = true;
			mg1.misses++;
		}
	}
	return false;
}

static void mg1_tick_anims(void)
{
	uint32_t now = SDL_GetTicks();
	if (now >= mg1.icon_t + 0x3c) {
		mg1.icon_t = now;
		mg1.icon_frame++;
		if (mg1.icon_frame >= (mg1.icon_state == 2 ? 4 : 6))
			mg1.icon_frame = 0;
	}
	for (int i = 0; i < mg1.nr_marks; ) {
		struct mg1_mark *m = &mg1.marks[i];
		if (now >= m->t + 20) {
			m->t = now;
			if (++m->frame >= 10) {
				memmove(m, m + 1, sizeof(*m) * (--mg1.nr_marks - i));
				continue;
			}
		}
		i++;
	}
	if (mg1.grade_kind >= 0 && now >= mg1.grade_t + 20 && mg1.grade_frame < 9) {
		mg1.grade_t = now;
		mg1.grade_frame++;
	}
	mg1_zoom_tick();
}

static void mg1_setup(struct param_list *params)
{
	if (mg1.sheet)
		SDL_FreeSurface(mg1.sheet);
	memset(&mg1, 0, sizeof(mg1));
	mg1.sheet = mg_sheet("mini01.gcc");
	mg1.chr = param_or(params, 2, 0);
	mg1.tool = param_or(params, 3, 0);
	mg1.zx = param_or(params, 4, 0);
	mg1.zy = param_or(params, 5, 0);
	mg1.anim_frames = 1;
	memset(mg1.itm, 0xff, sizeof(mg1.itm));
	mg1.grade_kind = -1;
	mg1.x = 8;
	mg_nr_voices = 0;
	mg1_draw(false);
	mg_show();
}

static void mg1_round(unsigned pattern)
{
	mem_set_var32(2, 0);
	if (!mg1.sheet)
		return;
	// knots of the pattern (0x430620): cell i at x = 0x21 + 8 i
	const char *p = mg1_patterns[pattern % 20];
	mg1.nr_notes = 0;
	int start = 0;
	for (int i = 0; i < 48; i++) {
		int x = 0x21 + i * 8;
		if (p[i] == '1')
			start = x + 7;
		else if (p[i] == '2' && mg1.nr_notes < 48)
			mg1.notes[mg1.nr_notes++] = (struct mg1_note) { start, x, false };
	}
	mg1.total += mg1.nr_notes * 2;
	mg1.x = 8;
	mg1.holding = false;
	mg1.round_score = 0;
	mg1.misses = 0;
	mg1.icon_state = 0;
	mg1.nr_marks = 0;
	mg1.grade_kind = -1;
	mg_input_reset();

	// the first round waits for [開始]
	while (!mg1.started) {
		mg1_tick_anims();
		mg1_draw(mg1_on_button());
		int e = mg_input_edge();
		if ((e > 0 && mg1_on_button()) || mg_debug_perfect()) {
			mg1.started = true;
			mg_wait_release();
		}
		mg_frame();
	}
	mg1_loop_se_start();
	mg1.step_t = SDL_GetTicks();
	bool ended = false;
	while (true) {
		uint32_t now = SDL_GetTicks();
		bool hover = mg1_on_button();
		int e = mg_input_edge();
		if (e > 0 && hover) {
			// [中断]
			mg1_loop_se_stop();
			audio_voice_stop(0);
			mem_set_var32(2, 1);
			return;
		}
		if (!ended && mg_debug_perfect()) {
			// test: press exactly at each knot, let go at its end
			e = 0;
			for (int i = 0; i < mg1.nr_notes; i++) {
				if (!mg1.holding && !mg1.notes[i].done && mg1.x + 8 == mg1.notes[i].start)
					e = 1;
				if (mg1.holding && mg1.x + 8 >= mg1.notes[i].end
						&& mg1.press_x == mg1.notes[i].start)
					e = -1;
			}
		}
		if (!ended) {
			if (e > 0 && !mg1.holding) {
				mg1.holding = true;
				mg1.press_x = mg1.x + 8;
				mg1.icon_state = 2;
				mg1.icon_frame = 0;
				mg1_zoom_press();
			} else if (e < 0 && mg1.holding) {
				mg1_release(mg1.x + 8);
			}
		}
		if (mg_debug_perfect() && now >= mg1.step_t + MG1_STEP_MS)
			mg1.step_t = now - MG1_STEP_MS; // one pixel per frame
		while (now >= mg1.step_t + MG1_STEP_MS) {
			mg1.step_t += MG1_STEP_MS;
			mg1.x++;
			if (!ended) {
				bool on = mg1_check_notes();
				if (!mg1.holding)
					mg1.icon_state = on ? 1 : 0;
			}
			if (mg1.x >= MG1_STOP_X)
				break;
		}
		// the original ends a round only at the end of the rope (no key
		// skips it: the launcher's skip button holds Ctrl)
		if (!ended && mg1.x > MG1_END_X - 1) {
			// end of the round (0x431500): grade, big mark, voice
			ended = true;
			if (mg1.holding)
				mg1_release(mg1.x + 8);
			for (int i = 0; i < mg1.nr_notes; i++) {
				if (!mg1.notes[i].done) {
					mg1.notes[i].done = true;
					mg1.misses++;
				}
			}
			mg1.icon_state = 0;
			mg1_loop_se_stop();
			mg1.score += mg1.round_score;
			int max = mg1.nr_notes * 2;
			int pct = max ? mg1.round_score * 100 / max : 0;
			int g = pct > 89 ? 2 : pct > 49 ? 1 : (mg1.misses <= mg1.nr_notes ? 0 : 3);
			mg1.grade_kind = g == 2 ? 0 : g == 1 ? 1 : 2;
			mg1.grade_frame = 0;
			mg1.grade_t = now;
			mg_voice_play(g, -1);
		}
		mg1_tick_anims();
		mg1_draw(hover);
		if (ended) {
			bool voice = audio_is_playing(AUDIO_CH_VOICE0);
			if ((mg1.x >= MG1_STOP_X && !voice) || mg_skip_keys())
				break;
		}
		mg_frame();
	}
	mg1.x = MG1_STOP_X;
	mg1.grade_kind = -1;
	mg1_draw(false);
}

// (66, 7, x, y): automatic -- the zoom animation while three voices play
static void mg1_auto(int zx, int zy)
{
	mg1.zx = zx;
	mg1.zy = zy;
	if (!mg1.sheet)
		mg1.sheet = mg_sheet("mini01.gcc");
	static const int kinds[3] = { 2, 1, 2 };
	for (int i = 0; i < 3; i++) {
		bool voice = mg_voice_play(kinds[i], -1);
		uint32_t t0 = SDL_GetTicks();
		mg1.holding = true;
		while (true) {
			mg1_zoom_tick();
			mg_clear();
			mg1_draw_zoom(mg1.zx, mg1.zy);
			if (input_down(INPUT_CANCEL)) {
				input_wait_until_up(INPUT_CANCEL);
				audio_voice_stop(0);
				mg1.holding = false;
				return;
			}
			if (mg_skip_keys())
				break;
			if (voice ? !audio_is_playing(AUDIO_CH_VOICE0) : SDL_GetTicks() > t0 + 1500)
				break;
			mg_frame();
		}
	}
	mg1.holding = false;
}

static void mg1_end(void)
{
	mg1_loop_se_stop();
	if (mg1.sheet)
		SDL_FreeSurface(mg1.sheet);
	mg1.sheet = NULL;
	mg_hide();
}

static void util_minigame1(struct param_list *params)
{
	switch (param_or(params, 1, 0)) {
	case 0: NOTICE("MGSTART 66"); input_touch_direct(true); mg1_setup(params); break;
	case 1: mg1_round(param_or(params, 2, 0)); break;
	case 2: mg1_end(); input_touch_direct(false); NOTICE("MGEND 66"); break;
	case 3: mg1.anim_frames = param_or(params, 2, 1); break;
	case 4:
		mg_voice_add(vm_string_param(params, 2), param_or(params, 3, 0), 0xff);
		break;
	case 5: {
		struct archive_data *d = asset_data_load(vm_string_param(params, 2));
		if (d) {
			memcpy(mg1.itm, d->data, min(d->size, 10u));
			archive_data_release(d);
		}
		break;
	}
	case 6: mg_score_result(mg1.score, mg1.total); break;
	case 7: mg1_auto(param_or(params, 2, 0), param_or(params, 3, 0)); break;
	}
}

/* ------------------------------------------------------------------ Util 67 */

#define MG2_BTN_X 0x204
#define MG2_BTN_Y 0x1a8
#define MG2_RING_MS 0x4b   // 0x46016d

static const int mg2_hot[3][2] = { { 8, 22 }, { 12, 22 }, { 18, 18 } };   // 0x597c98
static const int mg2_core[3][6] = {  // 0x597c50: w, h / src x, y / dst x, y
	{ 40, 40, 0, 40, 0, 0 }, { 32, 32, 0, 80, 4, 4 }, { 24, 24, 0, 112, 8, 8 },
};
static const uint8_t mg2_great[3][3][10] = {  // 0x597cb0 [level][kind][ring]
	{ { 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 0, 0, 1, 1 } },
	{ { 1, 1, 1 }, { 0, 0, 1, 1, 1 }, { 0, 0, 0, 0, 1, 1, 1 } },
	{ { 1, 1, 1, 1 }, { 0, 0, 1, 1, 1, 1 }, { 0, 0, 0, 0, 1, 1, 1, 1 } },
};
static const int mg2_ring_size[10] = { 40, 36, 32, 28, 24, 20, 16, 12, 8, 4 };
static const int mg2_ring_sx[10] = { 0, 40, 76, 108, 136, 160, 180, 196, 208, 216 };
static const char *const mg2_se[3][4] = {   // 0x5980b8: time out, miss, good, great
	{ "hh_144b.wav", "hh_144a.wav", "hh_146.wav", "hh_146b.wav" },
	{ "hh_139b.wav", "hh_139.wav", "hh_139c.wav", "hh_139d.wav" },
	{ "hh_128.wav", "hh_128.wav", "hh_129.wav", "hh_129.wav" },
};
// effects (0x598028 size, 0x598088 frames, 0x597d28 frame positions)
static const int mg2_eff_size[3][4][2] = {
	{ { 164, 320 }, { 164, 320 }, { 248, 424 }, { 248, 424 } },
	{ { 184, 180 }, { 184, 180 }, { 308, 316 }, { 308, 316 } },
	{ { 108, 104 }, { 156, 160 }, { 244, 240 }, { 244, 240 } },
};
static const int mg2_eff_frames[3][4] = { { 4, 4, 4, 8 }, { 4, 4, 4, 8 }, { 7, 7, 7, 7 } };
static const short mg2_eff_pos[3][4][8][2] = {
	{ { { 0, 0 }, { 164, 0 }, { 328, 0 }, { 492, 0 } },
	  { { 656, 0 }, { 820, 0 }, { 984, 0 }, { 1148, 0 } },
	  { { 0, 320 }, { 248, 320 }, { 496, 320 }, { 744, 320 } },
	  { { 0, 320 }, { 248, 320 }, { 496, 320 }, { 744, 320 },
	    { 0, 744 }, { 248, 744 }, { 496, 744 }, { 744, 744 } } },
	{ { { 0, 0 }, { 184, 0 }, { 368, 0 }, { 552, 0 } },
	  { { 0, 180 }, { 184, 180 }, { 368, 180 }, { 552, 180 } },
	  { { 0, 360 }, { 308, 360 }, { 616, 360 }, { 924, 360 } },
	  { { 0, 360 }, { 308, 360 }, { 616, 360 }, { 924, 360 },
	    { 0, 676 }, { 308, 676 }, { 616, 676 }, { 924, 676 } } },
	{ { { 0, 0 }, { 108, 0 }, { 216, 0 }, { 324, 0 }, { 432, 0 }, { 540, 0 }, { 648, 0 } },
	  { { 0, 104 }, { 156, 104 }, { 312, 104 }, { 468, 104 }, { 624, 104 }, { 780, 104 },
	    { 936, 104 } },
	  { { 0, 264 }, { 244, 264 }, { 488, 264 }, { 732, 264 }, { 0, 504 }, { 244, 504 },
	    { 488, 504 } },
	  { { 0, 264 }, { 244, 264 }, { 488, 264 }, { 732, 264 }, { 0, 504 }, { 244, 504 },
	    { 488, 504 } } },
};
static const int mg2_icons_pos[3][2] = { { 16, 16 }, { 524, 16 }, { 16, 424 } }; // 0x597d0c

static struct {
	SDL_Surface *target, *eff;
	int chr, tool, level, corner;
	int pos[10][2];
	uint8_t *dat;        // 640x480 region map, rows bottom-up
	int tx, ty, tn, kind;
	bool started;
	int score;           // 0xb0, out of 10 (0xb4)
	int rounds;          // 0xc4: icons used
	int ring, ring_round; // 0xbc, 0xb8
	uint32_t ring_t;
	bool tool_down;
} mg2;

static void mg2_draw_icons(void);

static void mg2_draw_base(void)
{
	mg_clear();
	mg2_draw_icons();
}

static void mg2_draw_icons(void)
{
	SDL_Surface *t = mg2.target;
	// remaining rounds (20x40 tool icons); the current one drains as a timer
	const int *ip = mg2_icons_pos[mg2.corner % 3];
	for (int i = mg2.rounds; i < 5; i++) {
		int x = ip[0] + i * 20, y = ip[1];
		if (i == mg2.rounds && mg2.started) {
			int r = (2 - mg2.ring_round) * 10 - mg2.ring;
			if (r > 0)
				mg_blit(t, (2 - mg2.tool) * 20, 0x88 + 40 - 2 * r, 20, 2 * r, x, y + 40 - 2 * r);
		} else {
			mg_blit(t, (2 - mg2.tool) * 20, 0x88, 20, 40, x, y);
		}
	}
}

static void mg2_draw_target(void)
{
	SDL_Surface *t = mg2.target;
	int x0 = mg2.tx - 20, y0 = mg2.ty - 20;
	const int *c = mg2_core[mg2.kind % 3];
	mg_blit(t, mg2.level * c[0] + c[2], c[3], c[0], c[1], x0 + c[4], y0 + c[5]);
	int r = mg2.ring;
	int off = (40 - mg2_ring_size[r]) / 2;
	mg_blit(t, mg2_ring_sx[r], 0, mg2_ring_size[r], mg2_ring_size[r], x0 + off, y0 + off);
}

static void mg2_draw_cursor(void)
{
	unsigned cx, cy;
	cursor_get_pos(&cx, &cy);
	if (cx >= MG2_BTN_X && cx < MG2_BTN_X + 112 && cy >= MG2_BTN_Y && cy < MG2_BTN_Y + 48)
		return;
	// the tool in target.gcc: x = tool * 128 + 0xdc (+0x40 pressed), y = 0x60
	int sx = mg2.tool * 0x80 + (mg2.tool_down ? 0x11c : 0xdc);
	mg_blit(mg2.target, sx, 0x60, 0x40, 0x40, cx - mg2_hot[mg2.tool][0], cy - mg2_hot[mg2.tool][1]);
}

static void mg2_draw_button(void)
{
	unsigned cx, cy;
	cursor_get_pos(&cx, &cy);
	bool hover = cx >= MG2_BTN_X && cx < MG2_BTN_X + 112 && cy >= MG2_BTN_Y && cy < MG2_BTN_Y + 48;
	int state = hover ? (input_down(INPUT_ACTIVATE) ? 2 : 1) : 0;
	mg_button(mg2.target, 0xdc + state * 0x70, mg2.started ? 0x30 : 0, MG2_BTN_X, MG2_BTN_Y,
			mg2.started);
}

// region under (x, y): 100 button, 10 target, 1..254 body, -1 nothing (0x45e0e0)
static int mg2_region(int x, int y)
{
	if (x > 0x203 && x < 0x275 && y > 0x1a7 && y < 0x1d9)
		return 100;
	if (!mg2.started)
		return -1;
	if (x >= mg2.tx - 20 && x < mg2.tx + 20 && y >= mg2.ty - 20 && y < mg2.ty + 20)
		return 10;
	if (!mg2.dat || x < 0 || x >= 640 || y < 0 || y >= 480)
		return -1;
	uint8_t v = mg2.dat[(479 - y) * 640 + x];
	return v == 0 || v == 0xff ? -1 : v;
}

// reaction face (surface 2..5, green transparent) over the screen
static void mg2_reaction_draw(int kind, int alpha)
{
	SDL_Surface *src = gfx_get_surface(kind + 2);
	SDL_Surface *ov = gfx_get_overlay(MG_OVERLAY);
	SDL_LockSurface(src);
	SDL_LockSurface(ov);
	uint32_t mask = mem_get_sysvar32(mes_sysvar32_mask_color);
	for (int y = 0; y < 480; y++) {
		uint32_t *d = (uint32_t*)((uint8_t*)ov->pixels + y * ov->pitch);
		for (int x = 0; x < 640; x++) {
			uint8_t r, g, b;
			uint8_t *p = (uint8_t*)src->pixels + y * src->pitch + x * src->format->BytesPerPixel;
			uint32_t px = src->format->BytesPerPixel == 4 ? *(uint32_t*)p
				: (uint32_t)p[0] | p[1] << 8 | p[2] << 16;
			SDL_GetRGB(px, src->format, &r, &g, &b);
			if ((r == 0 && g == 255 && b == 0)
					|| (r == ((mask >> 16) & 0xff) && g == ((mask >> 8) & 0xff) && b == (mask & 0xff)))
				continue;
			d[x] = SDL_MapRGBA(ov->format, r, g, b, alpha);
		}
	}
	SDL_UnlockSurface(ov);
	SDL_UnlockSurface(src);
}

// effect animation (0x45db00), 45 ms per frame, centered on (x, y)
static void mg2_effect(int kind, int x, int y)
{
	if (!mg2.eff)
		return;
	int t = mg2.tool;
	int w = mg2_eff_size[t][kind][0], h = mg2_eff_size[t][kind][1];
	for (int f = 0; f < mg2_eff_frames[t][kind]; f++) {
		uint32_t t0 = SDL_GetTicks();
		mg2_draw_base();
		mg2_draw_button();
		mg_blit(mg2.eff, mg2_eff_pos[t][kind][f][0], mg2_eff_pos[t][kind][f][1], w, h,
				x - w / 2, y - h / 2);
		// test switch HH_DEBUG_MINI_SLOW: effects 10 times slower
		while (SDL_GetTicks() < t0 + (getenv("HH_DEBUG_MINI_SLOW") ? 450 : 0x2d))
			mg_frame();
	}
}

// result of a round (0x45ee20): reaction face fades in, voice, wait
static void mg2_result(int effect, int reaction, int x, int y, bool *abort)
{
	mg2.rounds++;
	NOTICE("mini game 2: round %d result %d (score %d)", mg2.rounds, effect, mg2.score);
	mg2_effect(effect, x, y);
	bool voice = mg_voice_play(reaction, mg2.tn);
	uint32_t t0 = SDL_GetTicks();
	int alpha = 0;
	uint32_t at = t0;
	mg_input_reset();
	while (true) {
		mg_clear();
		if (SDL_GetTicks() >= at + 15 && alpha < 255) {
			at = SDL_GetTicks();
			alpha = min(alpha + 16, 255);
		}
		mg2_reaction_draw(reaction, alpha);
		mg2_draw_icons();
		mg2_draw_button();
		mg2_draw_cursor();
		int e = mg_input_edge();
		if (abort && e > 0 && mg2_region_cursor() == 100) {
			*abort = true;
			audio_voice_stop(0);
			break;
		}
		if (mg_skip_keys())
			break;
		if (voice ? !audio_is_playing(AUDIO_CH_VOICE0) : SDL_GetTicks() > t0 + 1200)
			break;
		mg_frame();
	}
	mg2_draw_base();
	mg2_draw_button();
}

static int mg2_region_cursor(void)
{
	unsigned cx, cy;
	cursor_get_pos(&cx, &cy);
	return mg2_region(cx, cy);
}

static void mg2_free(void)
{
	if (mg2.target)
		SDL_FreeSurface(mg2.target);
	if (mg2.eff)
		SDL_FreeSurface(mg2.eff);
	free(mg2.dat);
	mg2.target = mg2.eff = NULL;
	mg2.dat = NULL;
}

static void mg2_setup(struct param_list *params)
{
	mg2_free();
	memset(&mg2, 0, sizeof(mg2));
	mg2.chr = param_or(params, 2, 0);
	mg2.tool = ((int)param_or(params, 3, 5) - 5 + 3) % 3;
	mg2.level = min(param_or(params, 6, 0), 2u);
	mg2.corner = param_or(params, 7, 0) % 3;
	static const char *eff[3] = { "eff_lash.gcc", "eff_bamb.gcc", "eff_cand.gcc" };
	mg2.target = mg_sheet("target.gcc");
	mg2.eff = mg_sheet(eff[mg2.tool]);
	mg_nr_voices = 0;
	mg2_draw_base();
	mg2_draw_button();
	mg_show();
}

static void mg2_round(void)
{
	mem_set_var32(2, 0);
	if (!mg2.target)
		return;
	mg_input_reset();
	// [開始]
	while (!mg2.started) {
		mg2_draw_base();
		mg2_draw_button();
		if ((mg_input_edge() > 0 && mg2_region_cursor() == 100) || mg_debug_perfect()) {
			mg2.started = true;
			mg_wait_release();
		}
		mg_frame();
	}
	mg2.ring = 0;
	mg2.ring_round = 0;
	mg2.ring_t = SDL_GetTicks();
	mg2.tool_down = false;
	while (true) {
		uint32_t now = SDL_GetTicks();
		if (now >= mg2.ring_t + MG2_RING_MS) {
			mg2.ring_t = now;
			if (++mg2.ring > 9) {
				mg2.ring = 0;
				mg2.ring_round++;
			}
		}
		unsigned cx, cy;
		cursor_get_pos(&cx, &cy);
		if (mg2.ring_round >= 2) {
			// time out
			mg_se(mg2_se[mg2.tool][0]);
			mg2.score--;
			mg2_result(0, 0, cx, cy, NULL);
			return;
		}
		int e = mg_input_edge();
		mg2.tool_down = input_down(INPUT_ACTIVATE);
		if (mg_debug_perfect()) {
			// test: hit the target at the first ring step
			e = mg2_great[mg2.level][mg2.kind % 3][mg2.ring] ? 1 : 0;
			cx = mg2.tx;
			cy = mg2.ty;
		}
		if (e > 0) {
			int r = mg2_region(cx, cy);
			bool abort = false;
			if (r == 100) {
				NOTICE("mini game 2: aborted");
				mem_set_var32(2, 1);
				return;
			} else if (r == 10) {
				if (mg2_great[mg2.level][mg2.kind % 3][mg2.ring]) {
					mg_se(mg2_se[mg2.tool][3]);
					mg2.score += 2;
					mg2_result(3, 2, cx, cy, &abort);
				} else {
					mg_se(mg2_se[mg2.tool][2]);
					mg2.score += 1;
					mg2_result(2, 1, cx, cy, &abort);
				}
			} else if (r > 0) {
				mg_se(mg2_se[mg2.tool][1]);
				mg2.score--;
				mg2_result(1, 3, cx, cy, &abort);
			} else {
				goto draw;
			}
			if (abort)
				mem_set_var32(2, 1);
			return;
		}

draw:
		mg2_draw_base();
		mg2_draw_target();
		mg2_draw_button();
		mg2_draw_cursor();
		mg_frame();
	}
}

// (67, 8, tool): demonstration (0x45ebd0)
static void mg2_auto(void)
{
	static const int effects[5] = { 0, 1, 2, 2, 3 };
	static const int reactions[5] = { 0, 3, 1, 1, 2 };
	mg2.started = true;
	for (int i = 0; i < 5; i++) {
		int n = rand() % 10;
		mg_se(mg2_se[mg2.tool][effects[i]]);
		mg2_result(effects[i], reactions[i], mg2.pos[n][0], mg2.pos[n][1], NULL);
		if (input_down(INPUT_CANCEL)) {
			input_wait_until_up(INPUT_CANCEL);
			break;
		}
	}
}

static void util_minigame2(struct param_list *params)
{
	switch (param_or(params, 1, 0)) {
	case 0: NOTICE("MGSTART 67"); input_touch_direct(true); mg2_setup(params); break;
	case 1: mg2_round(); break;
	case 2: mg2_free(); mg_hide(); input_touch_direct(false); NOTICE("MGEND 67"); break;
	case 3: {
		unsigned n = param_or(params, 2, 0) % 10;
		mg2.tn = n;
		mg2.tx = mg2.pos[n][0];
		mg2.ty = mg2.pos[n][1];
		mg2.kind = param_or(params, 3, 0) % 3;
		break;
	}
	case 4:
		mg_voice_add(vm_string_param(params, 2), param_or(params, 3, 0),
				param_or(params, 4, 0xff));
		break;
	case 5: {
		struct archive_data *d = asset_data_load(vm_string_param(params, 2));
		if (d) {
			for (unsigned i = 0; i < 10 && (i + 1) * 8 <= d->size; i++) {
				mg2.pos[i][0] = le_get32(d->data, i * 8);
				mg2.pos[i][1] = le_get32(d->data, i * 8 + 4);
			}
			archive_data_release(d);
		}
		break;
	}
	case 6: {
		struct archive_data *d = asset_data_load(vm_string_param(params, 2));
		free(mg2.dat);
		mg2.dat = NULL;
		if (d && d->size >= 640 * 480) {
			mg2.dat = xmalloc(640 * 480);
			memcpy(mg2.dat, d->data, 640 * 480);
		}
		if (d)
			archive_data_release(d);
		break;
	}
	case 7: mg_score_result(mg2.score, 10); break;
	case 8:
		mg2.tool = ((int)param_or(params, 2, 5) - 5 + 3) % 3;
		mg2_auto();
		break;
	}
}
