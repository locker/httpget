#include <stddef.h>

#include "base64.h"

static const char *BASE64_TABLE =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static void append(int c, char *dst, size_t len, size_t *pos)
{
	if (*pos < len)
		dst[*pos] = c;
	++*pos;
}

static void encode_append(int c, char *dst, size_t len, size_t *pos)
{
	append(BASE64_TABLE[c], dst, len, pos);
}

size_t base64_encode(const char *src, char *dst, size_t len)
{
	size_t pos = 0;

	while (*src) {
		unsigned char a, b, c;

		a = *src++;
		encode_append(a >> 2, dst, len, &pos);
		if (*src) {
			b = *src++;
			encode_append(((a & 3) << 4) | (b >> 4),
				      dst, len, &pos);
			if (*src) {
				c = *src++;
				encode_append(((b & 15) << 2) | (c >> 6),
					      dst, len, &pos);
				encode_append(c & 63, dst, len, &pos);
			} else {
				encode_append((b & 15) << 2,
					      dst, len, &pos);
				append('=', dst, len, &pos);
			}
		} else {
			encode_append(((a & 3) << 4), dst, len, &pos);
			append('=', dst, len, &pos);
			append('=', dst, len, &pos);
		}
	}
	if (pos < len)
		dst[pos] = '\0';
	return pos;
}
