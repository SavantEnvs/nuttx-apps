/* dhcpd_env.h — force-included (-include) into netutils/dhcpd/dhcpd.c
 * (through dhcpd_session.c) only.  It swaps the calls the DHCP server makes
 * for host mocks of a LAN whose client messages are the fuzz input
 * (defined in dhcpd_session.c):
 *
 *   socket()      a placeholder descriptor (/dev/null), no real socket;
 *   setsockopt(), bind()   succeed;
 *   ioctl()       SIOCGIFADDR reports the server address 10.0.0.1, the
 *                 subnet of the CONFIG_NETUTILS_DHCPD_STARTIP pool;
 *   sendto()      OFFER/ACK/NAK replies: accepted, the last one kept for
 *                 the selftest;
 *   recv()        the next datagram of the input (one per call, truncated
 *                 to the caller's buffer).  When none is left it stops the
 *                 daemon the way its SIGWAKEUP handler does (ds_state =
 *                 DHCPD_STOP_REQUESTED) and fails with EINTR, so
 *                 dhcpd_run() returns;
 *   clock_gettime()   a fixed time, so lease expiry is deterministic.
 */
#ifndef __MAYHEM_DHCPD_ENV_H
#define __MAYHEM_DHCPD_ENV_H
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <time.h>
int dhcpd_env_socket(int domain, int type, int protocol);
int dhcpd_env_setsockopt(int fd, int level, int name, const void *val,
                         socklen_t len);
int dhcpd_env_bind(int fd, const struct sockaddr *addr, socklen_t len);
int dhcpd_env_ioctl(int fd, unsigned long req, unsigned long arg);
ssize_t dhcpd_env_sendto(int fd, const void *buf, size_t len, int flags,
                         const struct sockaddr *to, socklen_t tolen);
ssize_t dhcpd_env_recv(int fd, void *buf, size_t len, int flags);
int dhcpd_env_clock_gettime(clockid_t id, struct timespec *ts);
#define socket(d, t, p)                dhcpd_env_socket(d, t, p)
#define setsockopt(fd, lv, n, v, l)    dhcpd_env_setsockopt(fd, lv, n, v, l)
#define bind(fd, a, l)                 dhcpd_env_bind(fd, a, l)
#define ioctl(fd, r, a)                dhcpd_env_ioctl(fd, r, a)
#define sendto(fd, b, l, f, t, tl)     dhcpd_env_sendto(fd, b, l, f, t, tl)
#define recv(fd, b, l, f)              dhcpd_env_recv(fd, b, l, f)
#define clock_gettime(id, ts)          dhcpd_env_clock_gettime(id, ts)
#endif
