/* fuzz_netlib_url.c — netutils/netlib URL parsers.
 *
 * The whole input (NUL-terminated) is the URL, parsed the way the two
 * in-tree callers of each parser that this target covers do it, with their
 * buffers and sizes (Kconfig defaults):
 *
 *   netlib_parsehttpurl()  system/nxplayer/nxplayer.c (nxplayer's HTTP
 *                          stream URL): hostname[40] and relurl[100],
 *                          passed as sizeof() - 1;
 *   netlib_parseurl()      netutils/libcurl4nx/curl4nx_easy_setopt.c
 *                          (CURL4NXOPT_URL): scheme[16], host[128] and
 *                          path[128] (CONFIG_LIBCURL4NX_MAXHOST/MAXPATH).
 *
 * webclient's own netlib_parseurl() call (40/100-byte buffers) is reached by
 * the webclient target.  Each buffer is heap-allocated at its exact size, so
 * ASan sees an overflow of the real buffer.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "netutils/netlib.h"

#define NXPLAYER_HTTP_MAXHOSTNAME 40    /* CONFIG_NXPLAYER_HTTP_MAXHOSTNAME */
#define NXPLAYER_HTTP_MAXFILENAME 100   /* CONFIG_NXPLAYER_HTTP_MAXFILENAME */
#define CURL4NX_SCHEME            16    /* char scheme[16] */
#define CURL4NX_MAXHOST           128   /* CONFIG_LIBCURL4NX_MAXHOST */
#define CURL4NX_MAXPATH           128   /* CONFIG_LIBCURL4NX_MAXPATH */

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  char *url = malloc(size + 1);
  char *hostname = calloc(1, NXPLAYER_HTTP_MAXHOSTNAME);
  char *relurl = calloc(1, NXPLAYER_HTTP_MAXFILENAME);
  uint16_t port = 0;
  struct url_s u;

  memcpy(url, data, size);
  url[size] = '\0';

  netlib_parsehttpurl(url, &port, hostname, NXPLAYER_HTTP_MAXHOSTNAME - 1,
                      relurl, NXPLAYER_HTTP_MAXFILENAME - 1);
  free(relurl);
  free(hostname);

  memset(&u, 0, sizeof(u));
  u.schemelen = CURL4NX_SCHEME;
  u.hostlen = CURL4NX_MAXHOST;
  u.pathlen = CURL4NX_MAXPATH;
  u.scheme = malloc(u.schemelen);
  u.host = malloc(u.hostlen);
  u.path = malloc(u.pathlen);
  netlib_parseurl(url, &u);

  free(u.path);
  free(u.host);
  free(u.scheme);
  free(url);
  return 0;
}
