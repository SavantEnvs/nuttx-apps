/* fuzz_inifile.c — fsutils/inifile INI parser.
 *
 * The input is an INI file, written to a scratch file and opened with
 * inifile_initialize().  A fixed set of section/variable pairs is looked
 * up with inifile_read_string() and inifile_read_integer(), each lookup
 * rescanning the file (section headers, name=value lines, value parsing).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "fsutils/inifile.h"

static const char *const g_sections[] = { "section", "net", "a", "" };
static const char *const g_vars[] = { "name", "value", "ip", "x", "" };

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  const char *tmpdir = getenv("TMPDIR");
  char path[4096];
  INIHANDLE h;
  size_t i;
  size_t j;
  int fd;

  if (!tmpdir || !*tmpdir)
    {
      tmpdir = "/tmp";
    }

  snprintf(path, sizeof(path), "%s/inifile-XXXXXX", tmpdir);
  fd = mkstemp(path);
  if (fd < 0)
    {
      return 0;
    }

  if (write(fd, data, size) != (ssize_t)size)
    {
      close(fd);
      unlink(path);
      return 0;
    }

  close(fd);

  h = inifile_initialize(path);
  if (h)
    {
      for (i = 0; i < sizeof(g_sections) / sizeof(g_sections[0]); i++)
        {
          for (j = 0; j < sizeof(g_vars) / sizeof(g_vars[0]); j++)
            {
              char *s = inifile_read_string(h, g_sections[i], g_vars[j],
                                            "def");
              inifile_free_string(s);
              inifile_read_integer(h, g_sections[i], g_vars[j], 7);
            }
        }

      inifile_uninitialize(h);
    }

  unlink(path);
  return 0;
}
