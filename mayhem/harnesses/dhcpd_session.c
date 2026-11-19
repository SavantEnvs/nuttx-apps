/* dhcpd_session.c — netutils/dhcpd/dhcpd.c plus the host mocks of
 * dhcpd_env.h.  The mocks live in the same translation unit because the
 * end-of-input mock stops the daemon through its static g_dhcpd_daemon,
 * exactly as dhcpd_signal_handler() does on a stop request.
 */

#include "netutils/dhcpd/dhcpd.c"

#include <fcntl.h>

#undef socket
#undef setsockopt
#undef bind
#undef ioctl
#undef sendto
#undef recv
#undef clock_gettime

/* The current input, as a list of datagrams (set by fuzz_dhcpd.c). */

const uint8_t *g_dhcpd_env_data;
size_t g_dhcpd_env_size;

/* The replies sent so far and the last one (read by the selftest). */

int g_dhcpd_env_nsent;
uint8_t g_dhcpd_env_sent[sizeof(struct dhcpmsg_s)];
size_t g_dhcpd_env_sentlen;

int dhcpd_env_socket(int domain, int type, int protocol)
{
  (void)domain;
  (void)type;
  (void)protocol;
  return open("/dev/null", O_RDONLY | O_CLOEXEC);
}

int dhcpd_env_setsockopt(int fd, int level, int name, const void *val,
                         socklen_t len)
{
  (void)fd;
  (void)level;
  (void)name;
  (void)val;
  (void)len;
  return 0;
}

int dhcpd_env_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
  (void)fd;
  (void)addr;
  (void)len;
  return 0;
}

int dhcpd_env_ioctl(int fd, unsigned long req, unsigned long arg)
{
  FAR struct ifreq *ifr = (FAR struct ifreq *)arg;
  FAR struct sockaddr_in *in = (FAR struct sockaddr_in *)&ifr->ifr_addr;

  (void)fd;
  if (req != SIOCGIFADDR)
    {
      errno = ENOTTY;
      return -1;
    }

  memset(in, 0, sizeof(*in));
  in->sin_family = AF_INET;
  in->sin_addr.s_addr = htonl(0x0a000001);
  return 0;
}

ssize_t dhcpd_env_sendto(int fd, const void *buf, size_t len, int flags,
                         const struct sockaddr *to, socklen_t tolen)
{
  (void)fd;
  (void)flags;
  (void)to;
  (void)tolen;
  g_dhcpd_env_nsent++;
  g_dhcpd_env_sentlen = MIN(len, sizeof(g_dhcpd_env_sent));
  memcpy(g_dhcpd_env_sent, buf, g_dhcpd_env_sentlen);
  return len;
}

/* Datagrams are framed in the input as a 2-byte big-endian length followed
 * by that many bytes (the last one may be short: it is what is left). */

ssize_t dhcpd_env_recv(int fd, void *buf, size_t len, int flags)
{
  size_t n;

  (void)fd;
  (void)flags;
  if (g_dhcpd_env_size < 2)
    {
      g_dhcpd_daemon.ds_state = DHCPD_STOP_REQUESTED;
      errno = EINTR;
      return -1;
    }

  n = (size_t)g_dhcpd_env_data[0] << 8 | g_dhcpd_env_data[1];
  g_dhcpd_env_data += 2;
  g_dhcpd_env_size -= 2;
  if (n > g_dhcpd_env_size)
    {
      n = g_dhcpd_env_size;
    }

  memcpy(buf, g_dhcpd_env_data, n < len ? n : len);
  g_dhcpd_env_data += n;
  g_dhcpd_env_size -= n;
  return n < len ? n : len;
}

int dhcpd_env_clock_gettime(clockid_t id, struct timespec *ts)
{
  (void)id;
  ts->tv_sec = 1700000000;
  ts->tv_nsec = 0;
  return 0;
}
