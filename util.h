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
