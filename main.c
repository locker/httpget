#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
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
 * Used if -o option is omitted and URL ends with '/'.
 */
#define DEFAULT_OUTPUT_FILE	"index.html"

/*
 * Command line arguments.
 *
 * Initialized by parse_args() and never change afterwards.
 */
static char *PROG_NAME;
static char *URL;
static char *OUTPUT_FILE;

static int output_fd = -1;
static struct url_struct url;

static void print_usage(void)
{
	fprintf(stderr, "Usage: %1$s [option]... URL\n"
		"Try `%1$s -h' for more information\n",
		PROG_NAME);
}

static void print_help(void)
{
	printf("httpget - HTTP file retriever\n"
	       "Usage:\n"
	       "  %s [option]... URL\n"
	       "Options:\n"
	       "  -o FILE       write document to FILE\n"
	       "                (use `-' for stdandard output)\n"
	       "  -h            print this help and exit\n",
	       PROG_NAME);
}

static void parse_error(const char *fmt, ...)
{
	if (fmt) {
		va_list ap;

		fputs(PROG_NAME, stderr);
		fputs(": ", stderr);

		va_start(ap, fmt);
		vfprintf(stderr, fmt, ap);
		va_end(ap);

		fputc('\n', stderr);
	}
	print_usage();
	exit(2);
}

static void parse_args(int argc, char *argv[])
{
	int c;

	PROG_NAME = argv[0];

	while ((c = getopt(argc, argv, "o:h")) != -1) {
		switch (c) {
		case 'o':
			OUTPUT_FILE = optarg;
			break;
		case 'h':
			print_help();
			exit(0);
		default:
			parse_error(NULL);
		}
	}

	if (optind == argc)
		parse_error("URL missing");
	if (optind != argc - 1)
		parse_error("too many arguments");

	URL = argv[optind];
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

static void open_output_file(void)
{
	const char *filename = OUTPUT_FILE;

	if (!filename)
		filename = !strempty(url.name) ? url.name : DEFAULT_OUTPUT_FILE;
	else if (strcmp(OUTPUT_FILE, "-") == 0) {
		output_fd = STDOUT_FILENO;
		return;
	}

	output_fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, 0666);
	if (output_fd < 0)
		fail_errno("Failed to open output file");
}

static void close_output_file(void)
{
	if (output_fd != STDOUT_FILENO)
		close(output_fd);
}

static void output(const char *buf, size_t size)
{
	while (size > 0) {
		ssize_t n;

		n = write(output_fd, buf, size);
		if (n < 0)
			fail_errno("Failed to write to output file");

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

	open_output_file();
	while (1) {
		n = http_response_read(&resp, buf, BUF_SIZE);
		if (n < 0)
			fail("%s", http_last_error());
		if (!n)
			break;
		output(buf, n);
	}
	close_output_file();

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
	parse_args(argc, argv);
	download();
	exit(0);
}
