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

#ifndef AI5_SDL2_ENCODING_H
#define AI5_SDL2_ENCODING_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "nulib/string.h"

/*
 * Text encoding of the game's script (MES) text.
 *
 * Original games use Shift-JIS. Korean fan translations (e.g. Isaku98 with
 * Isaku98Loader.exe) store script text in CP949 instead; on Windows the loader
 * patches AI5WIN.EXE at runtime to handle this. We handle it here.
 */
enum text_encoding {
	TEXT_ENCODING_SJIS,
	TEXT_ENCODING_CP949,
};

extern enum text_encoding text_encoding;

static inline bool text_encoding_is_cp949(void)
{
	return text_encoding == TEXT_ENCODING_CP949;
}

// true if `b` starts a 2-byte (zenkaku) character in a TXT statement
bool ai5_char_is_zenkaku(uint8_t b);
// true if `b` is a valid 1-byte (hankaku) character in a STR statement
bool ai5_char_is_hankaku(uint8_t b);
// true if `b` is the first byte of a 2-byte character in a text string
bool ai5_char_is_2byte(uint8_t b);
// decode one character; returns pointer to the next character
const char *ai5_char_to_unicode(const char *src, int *dst);
// convert a script string to UTF-8
string ai5_cstring_to_utf8(const char *src, size_t len);

#endif // AI5_SDL2_ENCODING_H
