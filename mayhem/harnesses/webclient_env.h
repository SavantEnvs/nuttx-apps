/* webclient_env.h — force-included (-include) into
 * netutils/webclient/webclient.c only.  A response may redirect to a plain
 * http:// URL, which webclient opens with a real socket; on the host that
 * would reach the network, so connect() fails with ECONNREFUSED here (an
 * environment mock: the server is unreachable).  https:// goes through
 * the harness's tls_ops and keeps reading the fuzz input. */
#ifndef __MAYHEM_WEBCLIENT_ENV_H
#define __MAYHEM_WEBCLIENT_ENV_H
#include <errno.h>
#include <sys/socket.h>
static inline int wc_env_connect(int fd, const struct sockaddr *a,
                                 socklen_t l)
{
  (void)fd; (void)a; (void)l;
  errno = ECONNREFUSED;
  return -1;
}
/* Object-like on purpose: it also renames the tls_ops "connect" member
 * consistently within this one translation unit, leaving the layout alone. */
#define connect wc_env_connect
#endif
