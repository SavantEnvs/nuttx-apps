/* fuzz_webclient.c — netutils/webclient HTTP response handling.
 *
 * webclient_perform() GETs https://example.com/index.html.  The context's
 * tls_ops (the documented hook for a TLS stack) are a loopback mock: send
 * accepts the request, recv returns the fuzz input as the server's
 * response, then end of stream.  So the input is everything webclient
 * reads from a server: status line, headers (Content-Length, chunked
 * Transfer-Encoding, Location redirects, which reconnect and keep
 * reading the same stream), and body, which the sink callback discards.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "netutils/webclient.h"

struct stream_s
{
  const uint8_t *data;
  size_t size;
  size_t off;
};

static struct webclient_tls_connection *g_conn =
  (struct webclient_tls_connection *)&g_conn;

static int mock_connect(void *ctx, const char *host, const char *port,
                        unsigned int timeout,
                        struct webclient_tls_connection **connp)
{
  (void)ctx; (void)host; (void)port; (void)timeout;
  *connp = g_conn;
  return 0;
}

static ssize_t mock_send(void *ctx, struct webclient_tls_connection *conn,
                         const void *buf, size_t len)
{
  (void)ctx; (void)conn; (void)buf;
  return len;
}

static ssize_t mock_recv(void *ctx, struct webclient_tls_connection *conn,
                         void *buf, size_t len)
{
  struct stream_s *s = ctx;
  size_t n = s->size - s->off;

  (void)conn;
  if (n > len)
    {
      n = len;
    }

  memcpy(buf, s->data + s->off, n);
  s->off += n;
  return n;
}

static int mock_close(void *ctx, struct webclient_tls_connection *conn)
{
  (void)ctx; (void)conn;
  return 0;
}

static const struct webclient_tls_ops g_tls_ops =
{
  .connect = mock_connect,
  .send = mock_send,
  .recv = mock_recv,
  .close = mock_close,
};

static int sink(char **buffer, int offset, int datend, int *buflen,
                void *arg)
{
  (void)buffer; (void)offset; (void)datend; (void)buflen; (void)arg;
  return 0;
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  struct webclient_context ctx;
  struct stream_s s = { data, size, 0 };
  char buffer[512];

  webclient_set_defaults(&ctx);
  ctx.method = "GET";
  ctx.url = "https://example.com/index.html";
  ctx.buffer = buffer;
  ctx.buflen = sizeof(buffer);
  ctx.sink_callback = sink;
  ctx.tls_ops = &g_tls_ops;
  ctx.tls_ctx = &s;
  webclient_perform(&ctx);
  return 0;
}
