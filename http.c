#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <assert.h>

#include "util.h"
#include "http.h"

#define HTTP_PORT		80

#define HTTP_LINE_MAX		2048

#define BUF_SIZE		4096

/* Helpers for working with http_connection::buf */
#define BUF_BEGIN(conn)		((conn)->buf + (conn)->buf_begin)
#define BUF_END(conn)		((conn)->buf + (conn)->buf_end)
#define BUF_USED(conn)		((conn)->buf_end - (conn)->buf_begin)
#define BUF_LEFT(conn)		(BUF_SIZE - (conn)->buf_end)

/*
 * The last raised error is stored here, see http_last_error() and
 * set_last_error().
 *
 * XXX: This variable is not thread-safe! Consider making it TLS if we ever
 * need multi-threading.
 */
#define LAST_ERROR_MAX		256
static char last_error[LAST_ERROR_MAX];

static void set_last_error(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(last_error, LAST_ERROR_MAX, fmt, ap);
	va_end(ap);
}

static void set_last_error_errno(int err, const char *msg)
{
	char buf[64];

	strerror_r(err, buf, sizeof(buf));
	set_last_error("%s: %s", msg, buf);
}

const char *http_last_error(void)
{
	return last_error;
}

/*
 * Try to establish a tcp connection to be used for http session.
 * Return %true and set conn->sockfd on success.
 */
static bool do_connect(const char *host, int port,
		       struct http_connection *conn)
{
	struct addrinfo ai_hint;
	struct addrinfo *ai_result, *ai;
	char port_str[16];
	int sockfd;
	int err;

	memset(&ai_hint, 0, sizeof(ai_hint));
	ai_hint.ai_family = AF_UNSPEC;
	ai_hint.ai_socktype = SOCK_STREAM;

	snprintf(port_str, sizeof(port_str), "%d",
		 port >= 0 ? port : HTTP_PORT);

	err = getaddrinfo(host, port_str, &ai_hint, &ai_result);
	if (err) {
		set_last_error("Failed to translate address: %s",
			       gai_strerror(err));
		return false;
	}

	for (ai = ai_result; ai; ai = ai->ai_next) {
		sockfd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sockfd < 0) {
			err = errno;
			continue;
		}
		if (connect(sockfd, ai->ai_addr, ai->ai_addrlen) == 0)
			break;
		err = errno;
		close(sockfd);
	}

	freeaddrinfo(ai_result);

	if (!ai) {
		set_last_error_errno(err, "Failed to connect");
		return false;
	}

	conn->sockfd = sockfd;
	return true;
}

static void do_disconnect(struct http_connection *conn, int how)
{
	int err;

	err = shutdown(conn->sockfd, how);
	assert(err == 0);
}

/*
 * Wrapper around send(2). Sends exactly @len bytes from @buf on success. On
 * failure, sets @last_error and the @conn->failed flag. If the flag is already
 * set, does nothing.
 */
static void do_send(struct http_connection *conn, const char *buf, size_t len)
{
	while (!conn->failed && len > 0) {
		ssize_t n;

		n = send(conn->sockfd, buf, len,
			 MSG_NOSIGNAL);	/* don't want to die from SIGPIPE */
		if (n >= 0) {
			assert(n > 0);
			assert(n <= len);
			buf += n;
			len -= n;
		} else {
			set_last_error_errno(errno, "Send failed");
			conn->failed = true;
		}
	}
}

/*
 * Wrapper around recv(2). Receives @len bytes at max and stores them in @buf.
 * Returns the number of bytes received, which can be less than @len only if
 * the connection was closed or an error occurred. On failure, sets @last_error
 * and the @conn->failed flag. If the flag is already set, does nothing.
 */
static size_t do_recv(struct http_connection *conn, char *buf, size_t len)
{
	size_t ret = 0;

	while (!conn->failed && len > 0) {
		ssize_t n;

		n = recv(conn->sockfd, buf, len, 0);
		if (n > 0) {
			assert(n <= len);
			buf += n;
			len -= n;
			ret += n;
		} else if (n < 0) {
			set_last_error_errno(errno, "Receive failed");
			conn->failed = true;
		} else
			break; /* EOF */
	}
	return ret;
}

static void init_conn(struct http_connection *conn)
{
	memset(conn, 0, sizeof(*conn));
	conn->sockfd = -1;
	conn->buf = xmalloc(BUF_SIZE);
}

static void destroy_conn(struct http_connection *conn)
{
	if (conn->sockfd >= 0)
		close(conn->sockfd);
	free(conn->buf);
}

/*
 * Copy content of @buf to @conn->buf, max @len bytes.
 * Return the number of bytes actually copied.
 */
static size_t copy_to_buffer(struct http_connection *conn,
			     const char *buf, size_t len)
{
	size_t n;

	assert(conn->buf_end <= BUF_SIZE);

	n = min(BUF_LEFT(conn), len);
	if (n) {
		memcpy(BUF_END(conn), buf, n);
		conn->buf_end += n;
	}
	return n;
}

/*
 * Copy content of @conn->buf to @buf, max @len bytes.
 * Return the number of bytes actually copied.
 */
static size_t copy_from_buffer(struct http_connection *conn,
			       char *buf, size_t len)
{
	size_t n;

	assert(conn->buf_begin <= conn->buf_end);
	assert(conn->buf_end <= BUF_SIZE);

	n = min(BUF_USED(conn), len);
	if (n) {
		memcpy(buf, BUF_BEGIN(conn), n);
		conn->buf_begin += n;
	}
	return n;
}

/*
 * Send content of @conn->buf to the server.
 */
static void flush_buffer(struct http_connection *conn)
{
	assert(conn->buf_begin <= conn->buf_end);
	assert(conn->buf_end <= BUF_SIZE);

	do_send(conn, BUF_BEGIN(conn), BUF_USED(conn));
	conn->buf_begin = conn->buf_end = 0;
}

/*
 * Read data from socket and append them to @conn->buf.
 * Return the number of bytes read.
 */
static size_t refill_buffer(struct http_connection *conn)
{
	size_t n;

	assert(conn->buf_begin <= conn->buf_end);
	assert(conn->buf_end <= BUF_SIZE);

	if (!BUF_USED(conn))
		conn->buf_begin = conn->buf_end = 0;

	n = do_recv(conn, BUF_END(conn), BUF_LEFT(conn));
	conn->buf_end += n;
	return n;
}

/*
 * Buffered send. Appends content of @buf to @conn->buf until the latter is
 * full, then sends the whole buffer.
 */
static void buffered_send(struct http_connection *conn,
			  const char *buf, size_t len)
{
	while (!conn->failed && len > 0) {
		size_t n;

		n = copy_to_buffer(conn, buf, len);
		if (!n) {
			flush_buffer(conn);
			continue;
		}
		buf += n;
		len -= n;
	}
}

/*
 * Buffered recv. Well, it's not really buffered. The only difference of this
 * method from do_recv() is that it first copies data from @conn->buf if the
 * latter is not empty and only then proceeds to reading from socket.
 *
 * This function returns the number of copied to @buf, which can be less than
 * @len only if the connection was closed or an error occurred.
 *
 * This method is needed for reading response data, because we can't just use
 * do_recv() for the purpose as there might be some data left in the buffer
 * after receiving headers.
 */
static size_t buffered_recv(struct http_connection *conn, char *buf, size_t len)
{
	size_t ret = 0;

	/* First, copy from the buffer if it's not empty */
	if (BUF_USED(conn)) {
		ret = copy_from_buffer(conn, buf, len);
		buf += ret;
		len -= ret;
	}

	/* Still want more? */
	if (len > 0)
		ret += do_recv(conn, buf, len);

	return ret;
}

static void send_str(struct http_connection *conn, const char *str)
{
	buffered_send(conn, str, strlen(str));
}

static void send_header(struct http_connection *conn,
			const char *field, const char *value)
{
	send_str(conn, field);
	send_str(conn, ": ");
	send_str(conn, value);
	send_str(conn, "\r\n");
}

/*
 * Submit a http request. Return %true on success.
 */
static bool send_request(struct http_connection *conn,
			 const struct http_request_info *info)
{
	send_str(conn, info->command);
	send_str(conn, " ");
	send_str(conn, info->path);
	send_str(conn, " HTTP/1.1\r\n");

	/* Host header is mandatory in case of HTTP/1.1 */
	send_header(conn, "Host", info->host);

	/* We do not support persistent connections,
	 * neither do we actually need them for now */
	send_header(conn, "Connection", "close");

	send_str(conn, "\r\n");

	flush_buffer(conn);

	/* We only support bodiless requests, so disconnect now */
	do_disconnect(conn, SHUT_WR);

	return !conn->failed;
}

/*
 * Read a line ending with "\r\n" to @buf of size @buf_size. "\r\n" is not
 * copied to @buf. Return the line length.
 *
 * If the line doesn't fit in @buf, the line is truncated to @buf_size
 * characters and the function returns @buf_size. In this case @buf is not
 * nul-terminated.
 */
static size_t __recv_line(struct http_connection *conn,
			  char *buf, size_t buf_size)
{
	size_t line_len = 0;

	while (line_len < buf_size) {
		char *p;
		size_t n;

		if (!BUF_USED(conn) && !refill_buffer(conn))
			break; /* EOF */

		p = memchr(BUF_BEGIN(conn), '\n', BUF_USED(conn));

		n = p ? p - BUF_BEGIN(conn) : buf_size - line_len;
		n = copy_from_buffer(conn, buf + line_len, n);
		assert(n > 0);
		line_len += n;

		/* line separator found - we're done */
		if (p) {
			/* pop '\n' from the buffer */
			conn->buf_begin++;
			break;
		}
	}

	/* strip '\r' from the end; note, we don't complain if it's absent,
	 * i.e. we effectively accept "\n" as line separator */
	if (line_len > 0 && buf[line_len - 1] == '\r')
		line_len--;

	/* add terminating nul if there's space left */
	if (line_len < buf_size)
		buf[line_len] = '\0';

	return line_len;
}

/*
 * Size of @buf must equal %HTTP_LINE_MAX.
 */
static bool recv_line(struct http_connection *conn, char *buf)
{
	size_t n;

	n = __recv_line(conn, buf, HTTP_LINE_MAX);
	if (conn->failed)
		return false;
	if (n >= HTTP_LINE_MAX) {
		set_last_error("Invalid response: Header line too long");
		return false;
	}
	return true;
}

/*
 * Given a string supposedly containing a http response status line, try to
 * parse it. On success return %true and initialize @resp->version,
 * @resp->status, and @resp->reason accordingly.
 */
static bool parse_status(char *str, struct http_response *resp)
{
	char *p;

	if (strncasecmp(str, "HTTP/", 5) != 0)
		goto invalid_status;
	str += 5;

	/* skip version */
	p = findspace(str);

	resp->version = -1;
	/* all known versions consist of 3 characters */
	if (p - str == 3) {
		if (strncmp(str, "0.9", 3) == 0)
			resp->version = 9;
		if (strncmp(str, "1.0", 3) == 0)
			resp->version = 10;
		if (strncmp(str, "1.1", 3) == 0)
			resp->version = 11;
	}

	if (resp->version < 0) {
		*p = '\0';
		set_last_error("Invalid response: "
			       "Unknown protocol version: %s", str);
		goto fail;
	}

	str = skipspaces(p);
	resp->status = strtol(str, &p, 10);

	/* Status code consists of exactly 3 digits */
	if (p - str != 3 || !isspace(*p) || resp->status < 100)
		goto invalid_status;

	str = skipspaces(p);
	if (!*str) {
		set_last_error("Invalid response: "
			       "Reason message missing");
		goto fail;
	}

	resp->reason = xstrdup(str);
	return true;

invalid_status:
	set_last_error("Invalid response status");
fail:
	return false;
}

/*
 * Given a string supposedly containing a http header, try to parse it.
 * On success, return %true and initialize @field and @value.
 */
static bool parse_header(char *str, char **field, char **value)
{
	char *p;

	p = strchr(str, ':');
	if (!p) {
		set_last_error("Invalid response header: `:' missing");
		return false;
	}
	*p = '\0';

	*field = strstrip(str);
	if (strempty(*field)) {
		set_last_error("Invalid response header: Field name missing");
		return false;
	}

	*value = strstrip(p + 1);
	if (strempty(*value)) {
		set_last_error("Invalid response header: Value missing");
		return false;
	}

	return true;
}

/*
 * Receive a http response. Return %true on success.
 */
bool recv_response(struct http_connection *conn,
		   struct http_response *resp)
{
	char *buf;
	bool ret = false;

	buf = xmalloc(HTTP_LINE_MAX);

	/* Get status */
	if (!recv_line(conn, buf) ||
	    !parse_status(buf, resp))
		goto out;

	/* Proceed to the headers */
	while (1) {
		char *field, *value;

		if (!recv_line(conn, buf))
			goto err_hdrs;

		/* Empty line? Proceed to the message body */
		if (buf[0] == '\0')
			break;

		if (!parse_header(buf, &field, &value))
			goto err_hdrs;

		/* We do not need to interpret received headers for now */
	}

	ret = true;
out:
	free(buf);
	return ret;
err_hdrs:
	free(resp->reason);
	goto out;
}

bool http_simple_request(const struct http_request_info *info,
			 struct http_response *resp)
{
	struct http_connection *conn = &resp->conn;

	init_conn(conn);

	if (!do_connect(info->host, info->port, conn))
		goto fail;

	if (!send_request(conn, info))
		goto fail;

	if (!recv_response(conn, resp))
		goto fail;

	return true;
fail:
	destroy_conn(conn);
	return false;
}

ssize_t http_response_read(struct http_response *resp, void *buf, size_t len)
{
	return buffered_recv(&resp->conn, buf, len);
}

void http_response_destroy(struct http_response *resp)
{
	destroy_conn(&resp->conn);
	free(resp->reason);
}
