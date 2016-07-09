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

#endif /* _URL_H */
