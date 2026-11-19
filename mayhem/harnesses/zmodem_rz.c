/* zmodem_rz.c — the Zmodem receiver (system/zmodem, "rz") fed one input.
 *
 * Usage: zmodem_rz <input-file> [<directory>]
 *
 * The input is the byte stream a Zmodem sender writes to the serial line:
 * ZRQINIT/ZFILE/ZDATA/ZEOF/ZFIN headers (hex, binary16, binary32) and
 * ZDLE-escaped data subpackets.  rz_main.c opens a tty and calls
 * zmr_initialize(fd), zmr_receive(handle, dir) and zmr_release(handle);
 * this main() makes the same calls with fd being one end of a socketpair,
 * in one thread.  The sender's end is serviced from the receiver's own
 * read()/write() calls (zm_env_pump(), see zmodem_io.h): the input goes in,
 * then a half-close, and the replies are discarded.  The sender's EOF makes
 * zm_datapump() return -ENOTCONN, which ends the session.
 *
 * A file-input target rather than libFuzzer: the receiver arms its own
 * POSIX timer with a SIGALRM handler (zm_watchdog.c), which would replace
 * libFuzzer's.  The files a sender sends land in a scratch directory made
 * for this process and removed at exit (see zmodem_env.h), or, for
 * test.sh, in the existing <directory> given, which is kept.
 */

#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include "system/zmodem.h"

static char g_scratch[PATH_MAX];

static int confined(const char *path)
{
  size_t n = strlen(g_scratch);
  const char *p;

  if (strncmp(path, g_scratch, n) != 0 || path[n] != '/')
    {
      return 0;
    }

  for (p = path + n; *p != '\0'; )
    {
      size_t len;

      p += strspn(p, "/");
      len = strcspn(p, "/");
      if (len == 2 && p[0] == '.' && p[1] == '.')
        {
          return 0;
        }

      p += len;
    }

  return 1;
}

int zm_env_open(const char *path, int flags, ...)
{
  mode_t mode = 0;
  va_list ap;

  va_start(ap, flags);
  if (flags & O_CREAT)
    {
      mode = va_arg(ap, int);
    }

  va_end(ap);
  if ((flags & (O_ACCMODE | O_CREAT | O_TRUNC | O_APPEND)) != O_RDONLY &&
      !confined(path))
    {
      errno = EACCES;
      return -1;
    }

  return open(path, flags, mode);
}

int zm_env_unlink(const char *path)
{
  if (!confined(path))
    {
      errno = EACCES;
      return -1;
    }

  return unlink(path);
}

static int rm_entry(const char *path, const struct stat *sb, int type,
                    struct FTW *ftw)
{
  (void)sb;
  (void)type;
  (void)ftw;
  remove(path);
  return 0;
}

/* The sender's end of the socketpair and the input it has yet to send. */

static int g_remfd = -1;
static int g_peerfd = -1;
static const uint8_t *g_data;
static size_t g_left;
static int g_writing;

static void zm_env_pump(void)
{
  char sink[4096];

  while (recv(g_peerfd, sink, sizeof(sink), MSG_DONTWAIT) > 0)
    {
    }

  while (g_writing && g_left > 0)
    {
      ssize_t r = send(g_peerfd, g_data, g_left, MSG_NOSIGNAL | MSG_DONTWAIT);

      if (r <= 0)
        {
          if (r < 0 && errno == EINTR)
            {
              continue;
            }

          if (r < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            {
              return;           /* full: the receiver reads before more */
            }

          break;
        }

      g_data += r;
      g_left -= r;
    }

  if (g_writing)
    {
      shutdown(g_peerfd, SHUT_WR);
      g_writing = 0;
    }
}

ssize_t zm_env_read(int fd, void *buf, size_t len)
{
  if (fd == g_remfd)
    {
      zm_env_pump();
    }

  return read(fd, buf, len);
}

ssize_t zm_env_write(int fd, const void *buf, size_t len)
{
  ssize_t ret = write(fd, buf, len);

  if (fd == g_remfd)
    {
      int saved = errno;

      zm_env_pump();
      errno = saved;
    }

  return ret;
}

int main(int argc, char **argv)
{
  const char *tmp = getenv("TMPDIR");
  static uint8_t *data;
  size_t size = 0;
  size_t cap = 0;
  ZMRHANDLE handle;
  FILE *f;
  int sv[2];
  int ret = -1;

  if (argc < 2 || argc > 3 || (f = fopen(argv[1], "rb")) == NULL)
    {
      fprintf(stderr, "usage: %s <input-file> [<directory>]\n", argv[0]);
      return 2;
    }

  for (; ; )
    {
      if (size == cap)
        {
          cap = cap ? cap * 2 : 65536;
          data = realloc(data, cap);
        }

      size_t n = fread(data + size, 1, cap - size, f);
      if (n == 0)
        {
          break;
        }

      size += n;
    }

  fclose(f);

  if (argc == 3)
    {
      strlcpy(g_scratch, argv[2], sizeof(g_scratch));
    }
  else
    {
      snprintf(g_scratch, sizeof(g_scratch), "%s/rz.XXXXXX",
               tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
    }

  if ((argc == 2 && mkdtemp(g_scratch) == NULL) ||
      socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, sv) < 0)
    {
      perror("zmodem_rz");
      return 2;
    }

  g_remfd = sv[0];
  g_peerfd = sv[1];
  g_data = data;
  g_left = size;
  g_writing = 1;

  handle = zmr_initialize(sv[0]);
  if (handle != NULL)
    {
      ret = zmr_receive(handle, g_scratch);
      zmr_release(handle);
    }

  close(sv[0]);
  close(sv[1]);
  if (argc == 2)
    {
      nftw(g_scratch, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
    }

  free(data);
  return ret < 0 ? 1 : 0;
}
