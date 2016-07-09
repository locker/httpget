#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

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
