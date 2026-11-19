/* fuzz_codecs.c — netutils/codecs URL and base64 decoders.
 *
 * The first byte picks a decoder, the rest is the encoded text.  Output
 * buffers are sized the way the library's own *_len helpers say callers
 * must size them.  String-based decoders get a NUL-terminated copy (their
 * contract is a C string).
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "netutils/base64.h"
#include "netutils/urldecode.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  if (size < 1)
    {
      return -1;
    }

  unsigned sel = data[0];
  const char *src = (const char *)data + 1;
  size_t len = size - 1;
  char *str = malloc(len + 1);
  size_t outlen = 0;
  int dlen = 0;

  memcpy(str, src, len);
  str[len] = '\0';

  switch (sel % 7)
    {
      case 0: /* allocating base64 decode */
        free(base64_decode(src, len, NULL, &outlen));
        break;

      case 1: /* base64 decode into a caller buffer */
        {
          unsigned char *out = malloc(base64_decode_length(len) + 1);
          base64_decode(src, len, out, &outlen);
          free(out);
        }
        break;

      case 2: /* web-safe base64 decode */
        free(base64w_decode(src, len, NULL, &outlen));
        break;

      case 3: /* base64url (RFC 4648 section 5) */
        {
          size_t max = (sel >> 3) & 0x1f;
          unsigned char *out = malloc(max + 1);
          base64url_decode(str, out, max, &outlen);
          free(out);
        }
        break;

      case 4: /* length-delimited URL decode */
        {
          int n = urldecode_len(src, (int)len);
          char *out = malloc(n + 1);
          urldecode(src, (int)len, out, &dlen);
          free(out);
        }
        break;

      case 5: /* allocating URL decode */
        free(url_decode(str));
        break;

      case 6: /* in-place raw URL decode */
        urlrawdecode(str);
        break;
    }

  free(str);
  return 0;
}
