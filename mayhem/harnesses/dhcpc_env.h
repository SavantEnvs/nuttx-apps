/* dhcpc_env.h — force-included (-include) into netutils/dhcpc/dhcpc.c only.
 * It swaps the network calls the DHCP client makes for host mocks of a
 * network whose DHCP server's replies are the fuzz input (dhcpc_env.c):
 *
 *   socket()      a placeholder descriptor (/dev/null), no real socket;
 *   bind(), setsockopt()   succeed;
 *   sendto()      the client's DISCOVER/REQUEST/DECLINE: accepted, dropped;
 *   recv()        the next datagram of the input (UDP semantics: one
 *                 datagram per call, truncated to the caller's buffer).
 *                 When the input has none left it fails with ECONNRESET,
 *                 which dhcpc_request() treats as fatal, rather than with
 *                 the receive timeout (EAGAIN), which it would wait out
 *                 CONFIG_NETUTILS_DHCPC_RETRIES times;
 *   arc4random_buf()   zeros, so the transaction ID is 00 00 00 01 (the
 *                 client increments its last byte) and a reply can match.
 */
#ifndef __MAYHEM_DHCPC_ENV_H
#define __MAYHEM_DHCPC_ENV_H
#include <stdlib.h>
#include <sys/socket.h>
int dhcpc_env_socket(int domain, int type, int protocol);
int dhcpc_env_bind(int fd, const struct sockaddr *addr, socklen_t len);
int dhcpc_env_setsockopt(int fd, int level, int name, const void *val,
                         socklen_t len);
ssize_t dhcpc_env_sendto(int fd, const void *buf, size_t len, int flags,
                         const struct sockaddr *to, socklen_t tolen);
ssize_t dhcpc_env_recv(int fd, void *buf, size_t len, int flags);
void dhcpc_env_arc4random_buf(void *buf, size_t len);
#define socket(d, t, p)                dhcpc_env_socket(d, t, p)
#define bind(fd, a, l)                 dhcpc_env_bind(fd, a, l)
#define setsockopt(fd, lv, n, v, l)    dhcpc_env_setsockopt(fd, lv, n, v, l)
#define sendto(fd, b, l, f, t, tl)     dhcpc_env_sendto(fd, b, l, f, t, tl)
#define recv(fd, b, l, f)              dhcpc_env_recv(fd, b, l, f)
#define arc4random_buf(b, l)           dhcpc_env_arc4random_buf(b, l)
#endif
