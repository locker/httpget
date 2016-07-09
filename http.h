#ifndef _HTTP_H
#define _HTTP_H

#include <stddef.h>
#include <stdbool.h>
#include <stdarg.h>

typedef void (*http_dump_fn_t)(const char *, va_list);

extern http_dump_fn_t http_dump_fn;	/* if set, this function will be used
					   for dumping debug information */

struct http_connection {
	int sockfd;		/* tcp socket corresponding to the http connection */
	bool failed;		/* set if send/recv fails */

	char *buf;		/* on send: used for caching output;
				   on receive: used as buffer for received but
				   not yet processed data */
	size_t buf_begin;	/* index of the first actual byte in the buffer */
	size_t buf_end;		/* index of the byte following the last actual
				   byte in the buffer */
};

struct http_response {
	struct http_connection conn;

	int version;		/* http server protocol version;
				   9 for 0.9, 10 for 1.0, 11 for 1.1 */
	int status;		/* status code */
	char *reason;		/* reason message */

	size_t body_size;	/* content length; 0 if unavailable */
	size_t body_read;	/* number of bytes read from body */

	bool chunked;		/* Transfer-Encoding: chunked */
	size_t chunk_size;	/* number of bytes left in current chunk;
				   0 if we're done reading */
};

#define HTTP_STATUS_OK(status)	((status) / 100 == 2)	/* 2xx */

struct http_request_info {
	char *host;		/* http server host name */
	int port;		/* http server port number; -1 for auto */

	char *command;		/* http command, e.g. GET */
	char *path;		/* http command path */
};

/**
 * http_last_error - return the last error
 *
 * Returns the error message set by the last failed http_* method.
 * The caller must not modify the returned string.
 */
const char *http_last_error(void);

/**
 * http_simple_request - send a http request
 * @info: the request definition
 * @resp: the http response
 *
 * Returns %true and initializes @resp on success. On failure returns %false,
 * sets http_last_error(), and the content of @resp is undefined.
 *
 * In case the function succeeded, @resp may be used for reading response body
 * with http_response_read(). Finally, @resp is supposed to be destroyed with
 * http_response_destroy().
 */
bool http_simple_request(const struct http_request_info *info,
			 struct http_response *resp);

/**
 * http_response_read - read the body of a http response
 * @resp: the response
 * @buf: the buffer to write read data to
 * @len: the buffer length (maximal number of bytes to read)
 *
 * Returns the number of bytes read on success or -1 on error, in which case
 * http_last_error() is set accordingly. Return value of 0 means the end of
 * the stream was reached.
 */
ssize_t http_response_read(struct http_response *resp, void *buf, size_t len);

/**
 * http_response_destroy - destroy response returned by http_simple_request()
 * @resp: the response
 *
 * This function releases resources associated with @resp.
 */
void http_response_destroy(struct http_response *resp);

#endif /* _HTTP_H */
