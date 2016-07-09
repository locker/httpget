#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "http.h"
#include "url.h"
#include "util.h"

#define BUF_SIZE		65536

/*
 * Command line arguments.
 */
static char *PROG_NAME;
static char *URL;

static struct url_struct url;

static void print_usage(void)
{
	fprintf(stderr, "Usage: %s URL\n", PROG_NAME);
}

static void __fail(int err, const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);

	if (err) {
		char buf[64];

		strerror_r(err, buf, sizeof(buf));
		fputs(": ", stderr);
		fputs(buf, stderr);
	}

	fputc('\n', stderr);
	exit(1);
}

#define fail(fmt...)		__fail(0, fmt)
#define fail_errno(fmt...)	__fail(errno, fmt)

static void output(const char *buf, size_t size)
{
	while (size > 0) {
		ssize_t n;

		n = write(STDOUT_FILENO, buf, size);
		if (n < 0)
			fail_errno("Write failed");

		assert(n > 0);
		assert(n <= size);

		buf += n;
		size -= n;
	}
}

static void download_http(void)
{
	struct http_request_info info = {
		.host		= url.host,
		.port		= url.port,
		.command	= "GET",
		.path		= url.path,
	};
	struct http_response resp;
	char *buf;
	ssize_t n;

	buf = xmalloc(BUF_SIZE);

	if (!http_simple_request(&info, &resp))
		fail("%s", http_last_error());

	if (!HTTP_STATUS_OK(resp.status))
		fail("Error %d: %s", resp.status, resp.reason);

	while (1) {
		n = http_response_read(&resp, buf, BUF_SIZE);
		if (n < 0)
			fail("%s", http_last_error());
		if (!n)
			break;
		output(buf, n);
	}

	http_response_destroy(&resp);
	free(buf);
}

static void download(void)
{
	if (!url_parse(URL, &url))
		fail("Failed to parse URL");

	if (url.scheme && strcmp(url.scheme, "http") != 0)
		fail("URL scheme not supported: %s", url.scheme);

	if (!url.host)
		fail("Invalid URL: host name missing");

	download_http();
	url_destroy(&url);
}

int main(int argc, char *argv[])
{
	PROG_NAME = argv[0];

	if (argc != 2) {
		print_usage();
		exit(2);
	}

	URL = argv[1];

	download();
	exit(0);
}
