/* zmodem_io.h — force-included (-include) into system/zmodem/zm_state.c and
 * zm_utils.c, the only receiver sources that call read() and write().
 *
 * zmodem_rz.c drives the receiver from one thread.  The receiver's fd is
 * one end of a socketpair; zmodem_rz.c holds the other end (the sender).
 * Before each read() and after each write() on the receiver's fd,
 * zm_env_pump() acts for the sender without blocking: it discards the
 * receiver's replies, then sends more of the input, and half-closes once
 * the input is all sent.  read() then sees the input followed by EOF, just
 * as with a sender that writes everything and hangs up, and a reply never
 * waits on a full socket buffer.  Calls on other fds (the receiver's local
 * files) are untouched.
 */
#ifndef __MAYHEM_ZMODEM_IO_H
#define __MAYHEM_ZMODEM_IO_H
#include <sys/types.h>
#include <unistd.h>
ssize_t zm_env_read(int fd, void *buf, size_t len);
ssize_t zm_env_write(int fd, const void *buf, size_t len);
#define read(fd, buf, len)  zm_env_read(fd, buf, len)
#define write(fd, buf, len) zm_env_write(fd, buf, len)
#endif
