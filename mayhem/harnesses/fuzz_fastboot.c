/* fuzz_fastboot.c — system/fastboot daemon over its TCP transport.
 *
 * The input is the byte stream a fastboot client sends over TCP: the
 * "FB01" handshake, then 8-byte big-endian length frames carrying commands
 * (getvar:, download:, flash:, erase:, oem ..., upload, reboot) and
 * download payloads such as sparse images.  It reaches
 * fastboot_tcp_read(), fastboot_framed_read() and fastboot_read_all(), the
 * command dispatcher and the handlers, and the flash worker's sparse-image
 * programmer.
 *
 * Each input is one client connection to a fresh daemon context
 * (fastboot_session.c): the context, its download buffer, staged upload,
 * flash memfd and flash worker thread are created for the input and torn
 * down after it, so a result depends only on that input.  The connection is
 * a socketpair; the daemon's accept() returns its end (fastboot_env.c).  A
 * helper thread writes the input to the other end, half-closes it, and
 * drains the daemon's replies until the daemon closes the connection.
 *
 * A request cannot reach host files (fastboot_env.c), and reboot commands
 * reach the boardctl() stub, which fails.
 */

#include <poll.h>
#include <pthread.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

extern int g_fb_env_conn;
int fastboot_fuzz_session(void);

struct peer
{
  int fd;
  const uint8_t *data;
  size_t size;
};

static void *peer_main(void *arg)
{
  struct peer *p = arg;
  char sink[4096];
  int writing = 1;

  if (p->size == 0)
    {
      shutdown(p->fd, SHUT_WR);
      writing = 0;
    }

  for (; ; )
    {
      struct pollfd pfd;

      pfd.fd = p->fd;
      pfd.events = POLLIN | (writing ? POLLOUT : 0);
      if (poll(&pfd, 1, -1) < 0)
        {
          continue;
        }

      if (pfd.revents & (POLLIN | POLLHUP | POLLERR))
        {
          if (recv(p->fd, sink, sizeof(sink), MSG_DONTWAIT) == 0)
            {
              break;            /* the daemon closed the connection */
            }
        }

      if (writing && (pfd.revents & POLLOUT))
        {
          ssize_t r = send(p->fd, p->data, p->size,
                           MSG_NOSIGNAL | MSG_DONTWAIT);

          if (r > 0)
            {
              p->data += r;
              p->size -= r;
            }

          if (r < 0 || p->size == 0)
            {
              shutdown(p->fd, SHUT_WR);
              writing = 0;
            }
        }
    }

  return NULL;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct peer p;
  pthread_t th;
  int sv[2];

  if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0)
    {
      return 0;
    }

  g_fb_env_conn = sv[0];        /* taken (and closed) by the daemon */
  p.fd = sv[1];
  p.data = data;
  p.size = size;
  pthread_create(&th, NULL, peer_main, &p);

  fastboot_fuzz_session();
  if (g_fb_env_conn >= 0)       /* the daemon never accepted it */
    {
      close(g_fb_env_conn);
      g_fb_env_conn = -1;
    }

  pthread_join(th, NULL);
  close(sv[1]);
  return 0;
}
