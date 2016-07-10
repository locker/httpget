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

#ifndef _BASE64_H
#define _BASE64_H

#include <stddef.h>

/**
 * base64_encode - encode a string in base64
 * @src: the string to encode
 * @dst: the buffer to write the result to
 * @len: the buffer length
 *
 * Always returns the length of the encoded string, even if it doesn't fit in
 * the destination buffer.
 */
size_t base64_encode(const char *src, char *dst, size_t len);

#endif /* _BASE64_H */
