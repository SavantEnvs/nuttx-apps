/* fastboot_session.c — one fastboot client session on a fresh daemon
 * context, for fuzz_fastboot.c.
 *
 * fastboot_handler() (the daemon's entry point) never returns: its command
 * loop is "while (1)" around epoll_wait() on the TCP listener.  So this file
 * includes system/fastboot/fastboot.c (compiled with fastboot_env.h
 * force-included and main renamed) and runs the same steps per session:
 *
 *   fastboot_context_initialize()   download buffer, flash worker thread,
 *                                   TCP transport (fastboot_tcp_initialize)
 *   fastboot_create_publish()       the getvar variables
 *   the body of fastboot_command_loop(): ops->read() one frame, match it
 *                                   against g_fast_cmd[], call the handler
 *                                   (waiting for the async flash first,
 *                                   except for download:)
 *   fastboot_free_publish(), fastboot_context_deinit() and closing the
 *                                   flash memfd the worker left open.
 *
 * The session ends when the transport disconnects: fastboot_tcp_read()
 * closes the accepted socket when a read returns <= 0 (the peer's EOF).
 * Nothing in the context outlives the session.
 *
 * "oem memdump <addr> <size>" is not dispatched: it stages an arbitrary
 * address from the client for the next "upload" to read, a deliberate debug
 * read of device memory (there is no region check; CONFIG_BOARD_MEMORY_RANGE
 * only lists regions in its help text).  On the host that address is the
 * fuzzer's own memory, so its crashes are by design, not bugs.
 */

#include "system/fastboot/fastboot.c"

int fastboot_fuzz_session(void)
{
  struct fastboot_ctx_s ctx;
  char buffer[FASTBOOT_MSG_LEN + 1];
  bool started = false;
  int ret;

  memset(&ctx, 0, sizeof(ctx));
  ret = fastboot_context_initialize(&ctx, 1);
  if (ret < 0)
    {
      return ret;
    }

  fastboot_create_publish(&ctx, 1);

  for (; ; )
    {
      size_t ncmds = nitems(g_fast_cmd);
      size_t index;
      ssize_t r;

      if (started && ctx.tran_fd[1] < 0)
        {
          break;                /* disconnected */
        }

      started = true;
      r = ctx.ops->read(&ctx, buffer, FASTBOOT_MSG_LEN);
      if (r <= 0)
        {
          continue;
        }

      buffer[r] = '\0';
      if (r >= 11 && memcmp(buffer, "oem", 3) == 0 &&
          memcmp(buffer + 4, "memdump", 7) == 0)
        {
          fastboot_fail(&ctx, "Unknown command");
          continue;
        }

      for (index = 0; index < ncmds; index++)
        {
          size_t len = strlen(g_fast_cmd[index].prefix);

          if (memcmp(buffer, g_fast_cmd[index].prefix, len) == 0)
            {
              if (g_fast_cmd[index].handle != fastboot_download)
                {
                  while (sem_wait(&ctx.async.done) < 0);
                  sem_post(&ctx.async.done);
                }

              g_fast_cmd[index].handle(&ctx, buffer + len);
              break;
            }
        }

      if (index == ncmds)
        {
          fastboot_fail(&ctx, "Unknown command");
        }
    }

  fastboot_free_publish(&ctx, 1);
  fastboot_context_deinit(&ctx, 1);     /* waits for the flash worker */
  if (ctx.flash_fd >= 0)
    {
      close(ctx.flash_fd);
    }

  return 0;
}
