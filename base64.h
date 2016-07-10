#ifndef _BASE64_H
#define _BASE64_H

#include <stddef.h>

/**
 * base64_encode - encode a string in base64
 * @src: the string to encode
 * @dst: the buffer to write the result to
 * @len: the buffer length
 *
 * Always returns the length of the encoded string, even if it doesn't fit in
 * the destination buffer.
 */
size_t base64_encode(const char *src, char *dst, size_t len);

#endif /* _BASE64_H */
