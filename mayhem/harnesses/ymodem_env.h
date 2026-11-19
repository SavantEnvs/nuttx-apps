/* ymodem_env.h — force-included (-include) into system/ymodem/ymodem.c
 * only.  The serial line is the fuzz input (ymodem_env.c):
 *
 *   read()   the next bytes of the input.  When none are left it fails with
 *            ETIMEDOUT, the error ymodem_recv_file() retries and finally
 *            cancels on.  On a real line the read would instead block until
 *            the sender sends more (ymodem_recv() sets VMIN=255); returning
 *            0 would make ymodem_recv_buffer() spin forever, since it counts
 *            a 0-byte read as progress;
 *   write()  the receiver's C/ACK/NAK/CAN replies: accepted, dropped.
 */
#ifndef __MAYHEM_YMODEM_ENV_H
#define __MAYHEM_YMODEM_ENV_H
#include <sys/types.h>
#include <unistd.h>
ssize_t ym_env_read(int fd, void *buf, size_t len);
ssize_t ym_env_write(int fd, const void *buf, size_t len);
#define read(fd, b, n)   ym_env_read(fd, b, n)
#define write(fd, b, n)  ym_env_write(fd, b, n)
#endif
