/* fuzz_ymodem.c — system/ymodem, the YMODEM receiver ("rb").
 *
 * The input is the byte stream a YMODEM sender writes to the serial line:
 * SOH/STX packets (sequence number and its complement, a 128- or 1024-byte
 * payload, CRC-16), the file-name packet ("name\0length ..."), data packets,
 * EOT and CAN.  The harness calls ymodem_recv() with rb_main.c's defaults
 * (interval 15, retry 100, no custom packet size) and a packet handler that
 * does what rb_main.c's handler() does with each packet short of writing a
 * file: it takes the file name and length from a file-name packet, and from
 * a data packet the bytes up to the announced length (rb_main.c's size
 * calculation), which it reads.  The line is mocked by ymodem_env.h.
 *
 * Per input everything is fresh: the context and the handler's state are
 * reset, and ymodem_recv() allocates and frees its own packet buffer.
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "system/ymodem/ymodem.h"

struct rx
{
  size_t saved;               /* rb_main.c's file_saved_size */
  uint32_t sum;
};

extern const uint8_t *g_ym_env_data;
extern size_t g_ym_env_size;

static int handler(struct ymodem_ctx_s *ctx)
{
  struct rx *rx = ctx->priv;
  size_t size;
  size_t i;

  if (ctx->packet_type == YMODEM_FILENAME_PACKET)
    {
      rx->saved = 0;
      return strlen(ctx->file_name) > 0 ? 0 : -EINVAL;
    }

  if (rx->saved + ctx->packet_size > ctx->file_length)
    {
      size = ctx->file_length - rx->saved;
    }
  else
    {
      size = ctx->packet_size;
    }

  for (i = 0; i < size; i++)
    {
      rx->sum += ctx->data[i];
    }

  rx->saved += size;
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct ymodem_ctx_s ctx;
  struct rx rx;

  memset(&ctx, 0, sizeof(ctx));
  memset(&rx, 0, sizeof(rx));
  g_ym_env_data = data;
  g_ym_env_size = size;
  ctx.recvfd = -1;
  ctx.sendfd = -1;
  ctx.interval = 15;
  ctx.retry = 100;
  ctx.packet_handler = handler;
  ctx.priv = &rx;
  ymodem_recv(&ctx);
  return 0;
}
