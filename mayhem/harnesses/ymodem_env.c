/* ymodem_env.c — the serial-line mock of ymodem_env.h. */
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

/* The bytes the sender has still to send (set by the harness). */

const uint8_t *g_ym_env_data;
size_t g_ym_env_size;

ssize_t ym_env_read(int fd, void *buf, size_t len)
{
  (void)fd;
  if (g_ym_env_size == 0)
    {
      errno = ETIMEDOUT;
      return -1;
    }

  if (len > g_ym_env_size)
    {
      len = g_ym_env_size;
    }

  memcpy(buf, g_ym_env_data, len);
  g_ym_env_data += len;
  g_ym_env_size -= len;
  return len;
}

ssize_t ym_env_write(int fd, const void *buf, size_t len)
{
  (void)fd;
  (void)buf;
  return len;
}
