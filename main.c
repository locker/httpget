#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <limits.h>
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
static char *OUTPUT_FILE;	/* NULL for auto */
static ssize_t OUTPUT_POS;	/* -1 for auto */
static int MAX_REDIRECTIONS = 10;
static char *CREDS;
static bool TRUSTED_LOCATION;
static bool QUIET;

static int output_fd = -1;
static struct url_struct url;

static void printf_stderr(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
}

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
	       "  -c OFFSET	resume transfer at OFFSET\n"
	       "                (use `-' for auto detection)\n"
	       "  -r MAX_REDIR  max number of redirections\n"
	       "                (-1 for unlimited, default is %d)\n"
	       "  -u USER:PASS  server user and password\n"
	       "  -L            trust redirect location\n"
	       "  -q            quiet (no output)\n"
	       "  -v            increase output verbosity\n"
	       "                (useful for debugging)\n"
	       "  -h            print this help and exit\n",
	       PROG_NAME, MAX_REDIRECTIONS);
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
	long long x;

	PROG_NAME = argv[0];

	while ((c = getopt(argc, argv, "o:c:r:u:Lqvh")) != -1) {
		switch (c) {
		case 'o':
			OUTPUT_FILE = optarg;
			break;
		case 'c':
			if (strcmp(optarg, "-") == 0) {
				OUTPUT_POS = -1;
				break;
			}
			if (!strict_strtoll(optarg, 10, &x) ||
			    x < 0 || x > SSIZE_MAX)
				parse_error("invalid OFFSET");
			OUTPUT_POS = x;
			break;
		case 'r':
			if (!strict_strtoll(optarg, 10, &x) ||
			    x < -1 || x > INT_MAX)
				parse_error("invalid MAX_REDIR");
			MAX_REDIRECTIONS = x;
			break;
		case 'u':
			CREDS = optarg;
			break;
		case 'L':
			TRUSTED_LOCATION = true;
			break;
		case 'q':
			QUIET = true;
			break;
		case 'v':
			http_dump_fn = printf_stderr;
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

/*
 * Must be called before detect_output_pos(), because the latter needs to know
 * output file name.
 */
static void detect_output_file(void)
{
	/* Explicitly specified - nothing to do */
	if (OUTPUT_FILE)
		return;

	/* Otherwise use the last component of URL path */
	OUTPUT_FILE = !strempty(url.name) ? url.name : DEFAULT_OUTPUT_FILE;
}

/*
 * Not a part of open_output_file(), because we don't want to create a file
 * in case http request fails, while we need to know output position before
 * sending a request.
 */
static void detect_output_pos(void)
{
	struct stat st;

	/* Explicitly specified - nothing to do */
	if (OUTPUT_POS >= 0)
		return;

	/* Start from the beginning by default */
	OUTPUT_POS = 0;

	/*
	 * If we're writing to stdout there's no way to automatically resume
	 * the previous transfer.
	 */
	if (!OUTPUT_FILE)
		return;

	if (stat(OUTPUT_FILE, &st) == -1) {
		/* No file - no auto resume */
		if (errno != ENOENT)
			fail_errno("Failed to stat output file");
		return;
	}

	OUTPUT_POS = st.st_size;
}

static void open_output_file(void)
{
	int open_flags = O_WRONLY|O_CREAT;

	if (strcmp(OUTPUT_FILE, "-") == 0) {
		output_fd = STDOUT_FILENO;
		return;
	}

	/* Call ftruncate and lseek only if really necessary */
	if (OUTPUT_POS == 0)
		open_flags |= O_TRUNC;

	output_fd = open(OUTPUT_FILE, open_flags, 0666);
	if (output_fd < 0)
		fail_errno("Failed to open output file");

	if (OUTPUT_POS > 0) {
		if (ftruncate(output_fd, OUTPUT_POS) == -1)
			fail_errno("Failed to truncate output file");
		if (lseek(output_fd, OUTPUT_POS, SEEK_SET) == (off_t)-1)
			fail_errno("Seek on output file failed");
	}

	if (!QUIET) {
		fprintf(stderr, "Saving to: `%s`\n", OUTPUT_FILE);
		if (OUTPUT_POS > 0)
			fprintf(stderr, "Resuming transfer at %zd\n", OUTPUT_POS);
	}
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

/* Print @c @n times */
static void fputcn(int c, int n, FILE *stream)
{
	while (n-- > 0)
		fputc(c, stream);
}

static void print_progress(size_t read, size_t total, bool done)
{
	static time_t begin, last_update;
	static int line_len;

	time_t now;
	size_t rate;

	if (QUIET)
		return;

	now = time(NULL);

	/* Do not redraw more often than once a second */
	if (!done && now - last_update == 0)
		return;

	last_update = now;

	/* Convert to KB */
	read >>= 10;
	total >>= 10;

	/* Remove the previous progress line */
	fputc('\r', stderr);
	fputcn(' ', line_len, stderr);
	fputc('\r', stderr);

	if (!begin)
		begin = now;

	rate = read / (now - begin + 1);

	/*
	 * We can't terminate the line with '\n', because this way we couldn't
	 * easily redraw it by sending '\r'.
	 */
	line_len = fprintf(stderr,
			   "Downloaded %zu/%zu kB in %d second(s), "
			   "average rate: %zu kB/s",
			   read, total, (int)(now - begin), rate);
	if (total >= read)
		line_len += fprintf(stderr, ", time left: %d s",
				    (int)((total - read) / (rate + 1)));

	if (done)
		fputc('\n', stderr);
	else
		fflush(stderr);
}

static void download_http(void)
{
	struct http_request_info info = {
		.host		= url.host,
		.port		= url.port,
		.command	= "GET",
		.path		= url.path,
		.max_redirections = MAX_REDIRECTIONS,
		.creds		= CREDS,
		.trusted_location = TRUSTED_LOCATION,
	};
	struct http_response resp;
	char *buf;
	ssize_t n;

	detect_output_file();
	detect_output_pos();

	if (OUTPUT_POS > 0) {
		info.want_range = 1;
		info.range_first = OUTPUT_POS;
		info.range_last = SIZE_MAX;
	}

	buf = xmalloc(BUF_SIZE);

	if (!http_simple_request(&info, &resp))
		fail("%s", http_last_error());

	if (!HTTP_STATUS_OK(resp.status))
		fail("Error %d: %s", resp.status, resp.reason);

	if (info.want_range && !resp.ranged)
		fail("HTTP server does not seem to support byte ranges. "
		     "Cannot resume.");

	open_output_file();
	while (1) {
		n = http_response_read(&resp, buf, BUF_SIZE);
		print_progress(resp.body_read, resp.body_size, n <= 0);
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
