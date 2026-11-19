/* fastboot_env.h — force-included (-include) into system/fastboot/fastboot.c
 * (through fastboot_session.c) only.  It swaps four libc calls for host
 * mocks of the NuttX device environment the daemon expects (see
 * fastboot_env.c):
 *
 *   bind()  the TCP listener binds 127.0.0.1 on an ephemeral port instead
 *           of 0.0.0.0:5554, so parallel fuzzers never collide;
 *   accept() returns the harness's end of the input's connection (a
 *           socketpair), once; a second accept fails with EINVAL, as
 *           no other client will connect;
 *   open()  "/dev/<partition>" (a NuttX MTD/block device) becomes a fresh
 *           anonymous memfd; any other path fails with ENOENT, so a
 *           request can neither write nor read host files;
 *   stat()  the same policy for "oem filedump".
 */
#ifndef __MAYHEM_FASTBOOT_ENV_H
#define __MAYHEM_FASTBOOT_ENV_H
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
int fb_env_bind(int fd, const struct sockaddr *addr, socklen_t len);
int fb_env_accept(int fd, struct sockaddr *addr, socklen_t *len);
int fb_env_open(const char *path, int flags, ...);
int fb_env_stat(const char *path, struct stat *sb);
#define bind(fd, a, l) fb_env_bind(fd, a, l)
#define accept(fd, a, l) fb_env_accept(fd, a, l)
#define open(...)      fb_env_open(__VA_ARGS__)
#define stat(p, sb)    fb_env_stat(p, sb)
#endif
