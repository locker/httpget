/*
 * Miscellaneous helper functions.
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

#ifndef _UTIL_H
#define _UTIL_H

#include <stddef.h>
#include <stdbool.h>

#define min(x, y) ({				\
	typeof(x) _min1 = (x);			\
	typeof(y) _min2 = (y);			\
	(void) (&_min1 == &_min2);		\
	_min1 < _min2 ? _min1 : _min2; })

#define max(x, y) ({				\
	typeof(x) _max1 = (x);			\
	typeof(y) _max2 = (y);			\
	(void) (&_max1 == &_max2);		\
	_max1 > _max2 ? _max1 : _max2; })

static inline bool strempty(const char *str)
{
	return !*str;
}

/**
 * skipspaces - skip spaces in a string
 * @str: the string
 *
 * Returns the pointer to the first non-space character in @str. The
 * terminating nul is considered non-space.
 */
char *skipspaces(char *str);

/**
 * findspace - find first space character in a string
 * @str: the string
 *
 * Returns the pointer to the first space character in @str, or to the
 * terminating nul if there's no spaces in @str.
 */
char *findspace(char *str);

/**
 * strstrip - remove spaces from the beginning and the end of a string
 * @str: the string
 *
 * Returns a pointer to the first non-space character of @str.
 *
 * Note, this function may modify @str.
 */
char *strstrip(char *str);

/**
 * strict_strtoll - convert a string to long long
 * @str: the string (must be nul-terminated)
 * @base: the base (must be between 2 and 36 inclusive or 0)
 * @result: the result location
 *
 * Returns %true on success. On failure returns %false and sets errno.
 */
bool strict_strtoll(const char *str, int base, long long *result);

/**
 * str_seconds - print seconds to string
 * @seconds: the number of seconds
 * @buf: destination buffer
 * @size: the buffer size
 *
 * Returns @buf.
 *
 * This function outputs @seconds in a human readable form, e.g.
 * 12508 seconds -> "3h 28m 28s"
 */
char *str_seconds(unsigned int seconds, char *buf, size_t size);

/*
 * x versions of memory allocation functions never return NULL,
 * instead they terminate the program on failure
 */

void *__xmalloc(const char *, int, size_t);
void *__xstrdup(const char *, int, const char *);

#define xmalloc(size)		__xmalloc(__FILE__, __LINE__, (size))
#define xstrdup(s)		__xstrdup(__FILE__, __LINE__, (s))

/**
 * addrinfo_addr_port - extract address and port from addrinfo struct
 * @ai: the addrinfo struct
 * @addr: buffer to store the address string in
 * @len: size of @addr buffer
 * @port: location to write the port to
 *
 * Returns %true on success.
 */
struct addrinfo;
bool addrinfo_addr_port(struct addrinfo *ai,
			char *addr, size_t len, int *port);

#endif /* _UTIL_H */
