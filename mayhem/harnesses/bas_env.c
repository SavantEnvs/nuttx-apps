/* bas_env.c — the file-system policy of bas_env.h, and the per-process
 * scratch directory it confines writes to.
 *
 * Before main(), a fresh directory is made under $TMPDIR (default /tmp) and
 * becomes the working directory; it is removed again at exit().  The
 * program file argument is made absolute first, so a relative path given on
 * the command line still names the same file.
 */
#define _GNU_SOURCE 1
#include <errno.h>
#include <fcntl.h>
#include <ftw.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static char g_scratch[PATH_MAX];

static int confined(const char *path)
{
  const char *p = path;

  if (path == NULL || *path == '\0' || *path == '/')
    {
      return 0;
    }

  while (*p != '\0')
    {
      size_t n = strcspn(p, "/");

      if (n == 2 && p[0] == '.' && p[1] == '.')
        {
          return 0;
        }

      p += n;
      p += strspn(p, "/");
    }

  return 1;
}

static int refuse(void)
{
  errno = EACCES;
  return -1;
}

int bas_env_open(const char *path, int flags, ...)
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
      strcmp(path, "/dev/null") != 0 && !confined(path))
    {
      return refuse();
    }

  return open(path, flags, mode);
}

int bas_env_unlink(const char *path)
{
  return confined(path) ? unlink(path) : refuse();
}

int bas_env_rename(const char *from, const char *to)
{
  return confined(from) && confined(to) ? rename(from, to) : refuse();
}

int bas_env_mkdir(const char *path, mode_t mode)
{
  return confined(path) ? mkdir(path, mode) : refuse();
}

int bas_env_chdir(const char *path)
{
  (void)path;
  return refuse();
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

static void scratch_remove(void)
{
  if (g_scratch[0] != '\0')
    {
      nftw(g_scratch, rm_entry, 16, FTW_DEPTH | FTW_PHYS);
    }
}

__attribute__((constructor))
static void scratch_enter(int argc, char **argv, char **envp)
{
  const char *tmp = getenv("TMPDIR");
  int i;

  (void)envp;
  for (i = 1; i < argc; i++)
    {
      char *abs;

      if (argv[i][0] != '-' && argv[i][0] != '/' &&
          (abs = realpath(argv[i], NULL)) != NULL)
        {
          argv[i] = abs;
        }
    }

  snprintf(g_scratch, sizeof(g_scratch), "%s/bas.XXXXXX",
           tmp != NULL && *tmp != '\0' ? tmp : "/tmp");
  if (mkdtemp(g_scratch) == NULL || chdir(g_scratch) != 0)
    {
      perror("bas_env: scratch directory");
      abort();
    }

  atexit(scratch_remove);
}
