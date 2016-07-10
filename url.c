/*
 * URL parser.
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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "util.h"
#include "url.h"

static const char *slash_str = "/";

/*
 * Any of parse_* helpers returns true and advances str_ptr on success
 * (even if a non-mandatory token is absent), and false on failure.
 */

static bool parse_scheme(const char **str_ptr, struct url_struct *url)
{
	const char *begin = *str_ptr, *end, *s;
	char *out;

	for (s = begin; *s; s++) {
		/*
		 * Strictly speaking, URL scheme may only start with a letter,
		 * but we don't care.
		 */
		if (!(isalnum(*s) ||
		      *s == '+' || *s == '-' || *s == '.'))
			break;
	}

	/* scheme is missing - that's OK */
	if (s == begin || strncmp(s, "://", 3) != 0)
		return true;

	end = s;

	url->scheme = xmalloc(end - begin + 1);
	url->scheme[end - begin] = '\0';

	for (s = begin, out = url->scheme; s != end; s++, out++)
		*out = tolower(*s);

	*str_ptr = end + 3;
	return true;
}

static bool parse_host(const char **str_ptr, struct url_struct *url)
{
	const char *begin = *str_ptr, *end, *s;

	for (s = begin; *s; s++) {
		/*
		 * Again, not quite correct - see RFC 1123.
		 */
		if (!(isalnum(*s) || *s == '-' || *s == '.'))
			break;
	}

	/* hostname must be present if scheme is specified */
	if (s == begin)
		return !url->scheme;

	end = s;

	url->host = xmalloc(end - begin + 1);
	url->host[end - begin] = '\0';

	memcpy(url->host, begin, end - begin);

	*str_ptr = end;
	return true;
}

static bool parse_port(const char **str_ptr, struct url_struct *url)
{
	const char *s = *str_ptr;
	char *end;
	long val;

	/* no port - it's OK */
	if (*s != ':') {
		url->port = -1;
		return true;
	}

	/* port may only be specified along with hostname */
	if (!url->host)
		return false;

	s++;
	val = strtol(s, &end, 10);
	if (s == end || val < 0 || val > USHRT_MAX)
		return false;

	url->port = val;
	*str_ptr = end;
	return true;
}

static bool parse_path(const char **str_ptr, struct url_struct *url)
{
	const char *s = *str_ptr;

	/* empty string? */
	if (!*s && !url->host)
		return false;

	/* no path - assume `/' */
	if (!*s || (*s == '/' && !*(s + 1))) {
		url->path = (char *)slash_str;
		goto success;
	}

	if (*s != '/')
		return false;

	/* TODO: check path for invalid characters */
	url->path = xstrdup(s);

success:
	url->name = strrchr(url->path, '/');
	if (url->name)
		url->name += 1;
	else
		url->name = ""; /* we never free name so it's OK */
	return true;
}

bool url_parse(const char *str, struct url_struct *url)
{
	bool ret;

	memset(url, 0, sizeof(*url));
	ret = (parse_scheme(&str, url) &&
	       parse_host(&str, url) &&
	       parse_port(&str, url) &&
	       parse_path(&str, url));
	if (!ret)
		url_destroy(url);
	return ret;
}

void url_destroy(struct url_struct *url)
{
	free(url->scheme);
	free(url->host);
	if (url->path != slash_str)
		free(url->path);
}

struct url_struct *url_alloc(const char *str)
{
	struct url_struct *url;

	url = xmalloc(sizeof(*url));
	if (!url_parse(str, url)) {
		free(url);
		return NULL;
	}
	return url;
}

void url_free(struct url_struct *url)
{
	if (url) {
		url_destroy(url);
		free(url);
	}
}
