/* zmodem_env.h — force-included (-include) into system/zmodem/zm_receive.c
 * only.  The receiver creates, appends to and deletes the files a sender
 * names, under the directory given to zmr_receive().  zmodem_rz.c gives it
 * a scratch directory made for the process; these two calls go through
 * zmodem_rz.c, which refuses (EACCES) a path that is not inside that
 * directory or has a ".." component.  The sender's file name is otherwise
 * used as is: zmr_parsefilename() only rejects a leading '/'.
 *
 * sleep() is a no-op: the only call, in zmr_cmdsend()'s attention-string
 * loop (zm_receive.c), pauses one second per ATTNPSE (0xde) byte of the
 * string the sender sets with ZSINIT.  That pause is a timing gap on the
 * wire with no effect on parsing, and a sender could otherwise make each
 * input cost many seconds.
 */
#ifndef __MAYHEM_ZMODEM_ENV_H
#define __MAYHEM_ZMODEM_ENV_H
#include <fcntl.h>
#include <unistd.h>
int zm_env_open(const char *path, int flags, ...);
int zm_env_unlink(const char *path);
#define open(...)   zm_env_open(__VA_ARGS__)
#define unlink(p)   zm_env_unlink(p)
#define sleep(s)    ((void)(s), 0)
#endif
