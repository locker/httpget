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

#ifndef _URL_H
#define _URL_H

#include <stdbool.h>

/*
 * We assume that a valid URL looks like:
 *
 * [[scheme://]host[:port]][path]
 *
 * e.g.
 *
 * /path/to/file
 * example.com
 * http://localhost:80
 * http://localhost/index.html
 *
 * It doesn't exactly match RFC 3986 (e.g. we don't support credentials),
 * but it should do in most cases.
 */
struct url_struct {
	char *scheme;	/* always in lower case;
			   NULL if not specified */
	char *host;	/* NULL if not specified */
	char *path;	/* always starts with `/' */
	char *name;	/* last path component;
			   empty string if path ends with `/' */
	int port;	/* -1 if not specified */
};

/**
 * url_parse - parse a URL string
 * @str: the string
 * @url: where to store the result
 *
 * On success, url_parse() returns %true and initializes @url in accordance
 * with the given string. On failure, %false is returned, and @url content is
 * undefined. @url must be finally destroyed with url_destroy().
 *
 * Note, @url fields may be statically or dynamically allocated, so the caller
 * is not allowed to modify them.
 */
bool url_parse(const char *str, struct url_struct *url);

/**
 * url_destroy - destroy a URL returned by url_parse()
 * @url: the url to destroy
 */
void url_destroy(struct url_struct *url);

/**
 * url_alloc - alloc url_struct and init it from a URL string
 * @str: the string
 *
 * Returns a pointer to the new url_struct on success, or %NULL on failure.
 *
 * The new url_struct must be freed using url_free().
 */
struct url_struct *url_alloc(const char *str);

/**
 * url_free - free url_struct allocated by url_alloc()
 * @url: the url to free
 */
void url_free(struct url_struct *url);

#endif /* _URL_H */
