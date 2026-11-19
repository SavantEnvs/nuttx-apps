/* fuzz_nxinit_parser.c — system/nxinit RC-file parser.
 *
 * The input is an init.rc file.  It is parsed twice, each time with fresh
 * action/service managers wired exactly like init.c main() wires them:
 * once from memory with init_parse_config_buffer() and once from a file
 * with init_parse_config_file().  Both entry points run the line splitter,
 * init_parse_arguments() and the "on"/"service" section parsers plus the
 * service duplicate check.  Nothing is executed: no action is triggered and
 * no service is started.
 *
 * The "import" section is left out of the table on purpose: its argument is
 * a host path that init_parse_config_file() would open, so an input could
 * make the result depend on (or block on) files outside the input.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <nuttx/list.h>

#include "action.h"
#include "init.h"
#include "parser.h"
#include "service.h"

static void free_services(struct service_manager_s *sm)
{
  struct service_s *s, *tmp;
  struct service_class_s *c, *ctmp;
  int i;

  list_for_every_entry_safe(&sm->services, s, tmp, struct service_s, node)
    {
      list_for_every_entry_safe(&s->classes, c, ctmp,
                                struct service_class_s, node)
        {
          list_delete(&c->node);
          free(c);
        }

      for (i = 0; i < s->argc; i++)
        {
          free(s->argv[i]);
        }

      free(s->console);
      list_delete(&s->node);
      free(s);
    }
}

static void free_actions(struct action_manager_s *am)
{
  struct action_s *a, *atmp;
  struct action_cmd_s *cmd, *cmdtmp;
  size_t i;
  int j;

  list_for_every_entry_safe(&am->actions, a, atmp, struct action_s, node)
    {
      for (i = 0; i < nitems(a->events); i++)
        {
          free((void *)a->events[i].key);
          free(a->events[i].value);
        }

      list_for_every_entry_safe(&a->cmds, cmd, cmdtmp,
                                struct action_cmd_s, node)
        {
          for (j = 0; j < cmd->argc; j++)
            {
              free(cmd->argv[j]);
            }

          list_delete(&cmd->node);
          free(cmd);
        }

      list_delete(&a->node);
      free(a);
    }
}

static void run(const uint8_t *data, size_t size, const char *path)
{
  struct service_manager_s sm;
  struct action_manager_s am;

  memset(&sm, 0, sizeof(sm));
  list_initialize(&sm.services);
  memset(&am, 0, sizeof(am));
  list_initialize(&am.actions);
  list_initialize(&am.ready_actions);
  am.pid_running = -1;
  am.sm = &sm;

  const struct parser_s parser[] =
    {
      {"on", init_action_parse, NULL, &am},
      {"service", init_service_parse, init_service_check, &sm},
      {NULL},
    };

  if (path)
    {
      init_parse_config_file(parser, path);
    }
  else
    {
      init_parse_config_buffer(parser, (const char *)data, size);
    }

  free_actions(&am);
  free_services(&sm);
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  const char *tmpdir = getenv("TMPDIR");
  char path[4096];
  int fd;

  run(data, size, NULL);

  if (!tmpdir || !*tmpdir)
    {
      tmpdir = "/tmp";
    }

  snprintf(path, sizeof(path), "%s/nxinit-rc-XXXXXX", tmpdir);
  fd = mkstemp(path);
  if (fd < 0)
    {
      return 0;
    }

  if (write(fd, data, size) == (ssize_t)size)
    {
      close(fd);
      run(data, size, path);
    }
  else
    {
      close(fd);
    }

  unlink(path);
  return 0;
}
