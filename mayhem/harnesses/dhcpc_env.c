/* dhcpc_env.c — host mocks used by dhcpc.c (see dhcpc_env.h), and the
 * netlib interface-address calls dhcpc_request() makes. */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdint.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* The current input, as a list of datagrams (set by fuzz_dhcpc.c). */

const uint8_t *g_dhcpc_env_data;
size_t g_dhcpc_env_size;

/* The interface's IPv4 address, as netlib_set_ipv4addr() left it. */

struct in_addr g_dhcpc_env_ifaddr;

int dhcpc_env_socket(int domain, int type, int protocol)
{
  (void)domain;
  (void)type;
  (void)protocol;
  return open("/dev/null", O_RDONLY | O_CLOEXEC);
}

int dhcpc_env_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
  (void)fd;
  (void)addr;
  (void)len;
  return 0;
}

int dhcpc_env_setsockopt(int fd, int level, int name, const void *val,
                         socklen_t len)
{
  (void)fd;
  (void)level;
  (void)name;
  (void)val;
  (void)len;
  return 0;
}

ssize_t dhcpc_env_sendto(int fd, const void *buf, size_t len, int flags,
                         const struct sockaddr *to, socklen_t tolen)
{
  (void)fd;
  (void)buf;
  (void)flags;
  (void)to;
  (void)tolen;
  return len;
}

/* Datagrams are framed in the input as a 2-byte big-endian length followed
 * by that many bytes (the last one may be short: it is what is left). */

ssize_t dhcpc_env_recv(int fd, void *buf, size_t len, int flags)
{
  size_t n;

  (void)fd;
  (void)flags;
  if (g_dhcpc_env_size < 2)
    {
      errno = ECONNRESET;
      return -1;
    }

  n = (size_t)g_dhcpc_env_data[0] << 8 | g_dhcpc_env_data[1];
  g_dhcpc_env_data += 2;
  g_dhcpc_env_size -= 2;
  if (n > g_dhcpc_env_size)
    {
      n = g_dhcpc_env_size;
    }

  memcpy(buf, g_dhcpc_env_data, n < len ? n : len);
  g_dhcpc_env_data += n;
  g_dhcpc_env_size -= n;
  return n < len ? n : len;
}

void dhcpc_env_arc4random_buf(void *buf, size_t len)
{
  memset(buf, 0, len);
}

int netlib_get_ipv4addr(const char *ifname, struct in_addr *addr)
{
  (void)ifname;
  *addr = g_dhcpc_env_ifaddr;
  return 0;
}

int netlib_set_ipv4addr(const char *ifname, const struct in_addr *addr)
{
  (void)ifname;
  g_dhcpc_env_ifaddr = *addr;
  return 0;
}
