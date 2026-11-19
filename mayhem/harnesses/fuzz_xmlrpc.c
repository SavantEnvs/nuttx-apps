/* fuzz_xmlrpc.c — netutils/xmlrpc request parser.
 *
 * The input is an XML-RPC request body, handed to xmlrpc_parse() as the
 * server does after reading it off a socket (a mutable NUL-terminated
 * buffer).  One method, "echo", is registered; it reads every argument
 * with the typed getters and builds a response, so the argument parser,
 * the getters and xmlrpc_buildresponse() all run.  The response is sent to
 * an invalid descriptor (-1), so nothing leaves the process.
 *
 * The body is parsed from a zero-filled buffer of
 * CONFIG_EXAMPLES_XMLRPC_BUFFERSIZE (1024, the Kconfig default) bytes,
 * truncated to fit with a NUL, exactly as examples/xmlrpc/xmlrpc_main.c
 * hands it to xmlrpc_parse().  xmlrpc_getelement() scans past the string's
 * NUL into that buffer; with an exact-size heap copy every unterminated
 * input was an over-read and Mayhem could not fuzz 5 iterations.  Reads
 * past the end of the server's buffer are still reported.
 *
 * Kept across inputs on purpose: the method registration (read-only) and
 * the library's static g_xmlcall, which xmlrpc_parse() resets with memset
 * before parsing each methodCall.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "netutils/xmlrpc.h"

/* CONFIG_EXAMPLES_XMLRPC_BUFFERSIZE default (examples/xmlrpc/Kconfig). */

#define XMLRPC_HTTP_BUFSIZE 1024

static int echo(struct xmlrpc_s *call)
{
  char s[CONFIG_XMLRPC_STRINGSIZE + 1];
  double d;
  int i;
  int n;

  for (n = 0; n < call->argsize; n++)
    {
      switch (call->args[n])
        {
          case 'i':
            if (xmlrpc_getinteger(call, &i) == 0)
              {
                xmlrpc_buildresponse(call, "{i}", "int", i);
              }
            break;

          case 'b':
            if (xmlrpc_getbool(call, &i) == 0)
              {
                xmlrpc_buildresponse(call, "b", i);
              }
            break;

          case 'd':
            if (xmlrpc_getdouble(call, &d) == 0)
              {
                xmlrpc_buildresponse(call, "d", d);
              }
            break;

          case 's':
            if (xmlrpc_getstring(call, s) == 0)
              {
                xmlrpc_buildresponse(call, "{s}", "str", s);
              }
            break;
        }
    }

  return 0;
}

static struct xmlrpc_entry_s g_echo =
{
  .func = echo,
  .name = "echo",
};

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  static int registered;
  char *buf;
  size_t n = size < XMLRPC_HTTP_BUFSIZE - 1 ? size : XMLRPC_HTTP_BUFSIZE - 1;

  if (!registered)
    {
      xmlrpc_register(&g_echo);
      registered = 1;
    }

  buf = calloc(1, XMLRPC_HTTP_BUFSIZE);
  memcpy(buf, data, n);
  xmlrpc_parse(-1, buf);
  free(buf);
  return 0;
}
