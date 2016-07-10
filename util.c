#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <assert.h>

#include "util.h"

char *skipspaces(char *str)
{
	while (isspace(*str))
		str++;
	return str;
}

char *findspace(char *str)
{
	while (*str && !isspace(*str))
		str++;
	return str;
}

char *strstrip(char *str)
{
	char *p;

	str = skipspaces(str);

	p = str + strlen(str);
	while (p != str && isspace(*(p - 1)))
		p--;
	*p = '\0';

	return str;
}

bool strict_strtoll(const char *str, int base, long long *result)
{
	char *end;

	assert(base == 0 || (base >= 2 && base <= 36));

	*result = strtoll(str, &end, base);
	if ((*result == LLONG_MIN || *result == LLONG_MAX) && errno == ERANGE)
		return false;
	if (end == str || *end) {
		errno = -EINVAL;
		return false;
	}
	return true;
}

char *str_seconds(unsigned int seconds, char *buf, size_t size)
{
	unsigned int hours, minutes;

	hours = seconds / 60 / 60;
	minutes = seconds / 60 - hours * 60;
	seconds = seconds - minutes * 60 - hours * 60 * 60;

	if (hours)
		snprintf(buf, size, "%uh %um %us", hours, minutes, seconds);
	else if (minutes)
		snprintf(buf, size, "%um %us", minutes, seconds);
	else
		snprintf(buf, size, "%us", seconds);

	return buf;
}

static void __xalloc_failed(const char *file, int line, size_t size)
{
	fprintf(stderr, "%s:%d: Failed to allocate memory block of size %zu\n",
		file, line, size);
	_exit(64);
}

#define __XALLOC(name, size, ...)					\
	void *p = name(__VA_ARGS__);					\
	if (!p)								\
		__xalloc_failed(_file, _line, (size));			\
	return p;

void *__xmalloc(const char *_file, int _line, size_t size)
{
	__XALLOC(malloc, size, size);
}

void *__xstrdup(const char *_file, int _line, const char *s)
{
	__XALLOC(strdup, strlen(s) + 1, s);
}

bool addrinfo_addr_port(struct addrinfo *ai,
			char *addr, size_t len, int *port)
{
	void *addr_raw;
	unsigned short port_raw;

	switch (ai->ai_family) {
	case AF_INET:
		addr_raw = &((struct sockaddr_in *)ai->ai_addr)->sin_addr;
		port_raw = ((struct sockaddr_in *)ai->ai_addr)->sin_port;
		break;
	case AF_INET6:
		addr_raw = &((struct sockaddr_in6 *)ai->ai_addr)->sin6_addr;
		port_raw = ((struct sockaddr_in6 *)ai->ai_addr)->sin6_port;
		break;
	default:
		errno = EAFNOSUPPORT;
		return false;
	}

	if (!inet_ntop(ai->ai_family, addr_raw, addr, len))
		return false;

	*port = ntohs(port_raw);
	return true;
}
