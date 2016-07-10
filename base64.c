/*
 * Library for encoding text strings in base64 format.
 *
 * Copyright (C) 2016  Vladimir Davydov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stddef.h>

#include "base64.h"

static const char *BASE64_TABLE =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void append(int c, char *dst, size_t len, size_t *pos)
{
	if (*pos < len)
		dst[*pos] = c;
	++*pos;
}

static void encode_append(int c, char *dst, size_t len, size_t *pos)
{
	append(BASE64_TABLE[c], dst, len, pos);
}

size_t base64_encode(const char *src, char *dst, size_t len)
{
	size_t pos = 0;

	while (*src) {
		unsigned char a, b, c;

		a = *src++;
		encode_append(a >> 2, dst, len, &pos);
		if (*src) {
			b = *src++;
			encode_append(((a & 3) << 4) | (b >> 4),
				      dst, len, &pos);
			if (*src) {
				c = *src++;
				encode_append(((b & 15) << 2) | (c >> 6),
					      dst, len, &pos);
				encode_append(c & 63, dst, len, &pos);
			} else {
				encode_append((b & 15) << 2,
					      dst, len, &pos);
				append('=', dst, len, &pos);
			}
		} else {
			encode_append(((a & 3) << 4), dst, len, &pos);
			append('=', dst, len, &pos);
			append('=', dst, len, &pos);
		}
	}
	if (pos < len)
		dst[pos] = '\0';
	return pos;
}
