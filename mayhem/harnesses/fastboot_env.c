/* fastboot_env.c — host mocks used by fastboot.c (see fastboot_env.h). */
#define _GNU_SOURCE 1
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>

/* The daemon's end of the current input's connection (fuzz_fastboot.c). */

int g_fb_env_conn = -1;

int fb_env_bind(int fd, const struct sockaddr *addr, socklen_t len)
{
  struct sockaddr_in in;

  (void)addr;
  (void)len;
  memset(&in, 0, sizeof(in));
  in.sin_family = AF_INET;
  in.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  in.sin_port = 0;
  return bind(fd, (struct sockaddr *)&in, sizeof(in));
}

int fb_env_accept(int fd, struct sockaddr *addr, socklen_t *len)
{
  int conn = g_fb_env_conn;

  (void)fd;
  (void)addr;
  (void)len;
  if (conn < 0)
    {
      /* The input is the only client.  EINVAL (a listener that no longer
       * accepts), not EAGAIN: fastboot_download() retries EAGAIN forever,
       * waiting for a client that will never come.
       */

      errno = EINVAL;
      return -1;
    }

  g_fb_env_conn = -1;
  return conn;
}

int fb_env_open(const char *path, int flags, ...)
{
  (void)flags;
  if (strncmp(path, "/dev/", 5) == 0)
    {
      return memfd_create("fastboot-partition", MFD_CLOEXEC);
    }

  errno = ENOENT;
  return -1;
}

int fb_env_stat(const char *path, struct stat *sb)
{
  (void)path;
  (void)sb;
  errno = ENOENT;
  return -1;
}
