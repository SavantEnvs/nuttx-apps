/* fuzz_minibasic.c — interpreters/minibasic.
 *
 * The input is a BASIC program.  It is NUL-terminated and run with
 * basic(script, in, out, err) as script.c does after loading a file, with
 * INPUT reading from /dev/null and PRINT/error output discarded to
 * /dev/null.  The interpreter has no file or shell statements.  A program
 * that loops forever is a hang, which Mayhem's per-run timeout reports.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "interpreters/minibasic.h"

static FILE *g_in;
static FILE *g_out;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  char *script;

  if (!g_in)
    {
      g_in = fopen("/dev/null", "r");
      g_out = fopen("/dev/null", "w");
    }

  /* INPUT must see the same (empty) stream on every run. */

  clearerr(g_in);
  rewind(g_in);

  script = malloc(size + 1);
  memcpy(script, data, size);
  script[size] = '\0';
  basic(script, g_in, g_out, g_out);
  free(script);
  return 0;
}
