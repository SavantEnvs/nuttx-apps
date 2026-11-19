/* selftest.c — known-answer tests for the nuttx-apps modules the fuzz
 * targets cover besides nxinit (which has upstream's own cmocka suite).
 * Built by mayhem/build.sh with normal flags, run by mayhem/test.sh.
 * Prints "PASS <name>" / "FAIL <name>" per test and a final
 * "selftest: <passed> passed, <failed> failed" line.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "fsutils/inifile.h"
#include "interpreters/minibasic.h"
#include <arpa/inet.h>
#include <errno.h>

#include "netutils/base64.h"
#include "netutils/dhcpc.h"
#include "netutils/dhcpd.h"
#include "netutils/netlib.h"
#include "netutils/urldecode.h"
#include "netutils/xmlrpc.h"
#include "system/ymodem/ymodem.h"

static int g_pass;
static int g_fail;

static void check(const char *name, int ok)
{
  printf("%s %s\n", ok ? "PASS" : "FAIL", name);
  if (ok)
    {
      g_pass++;
    }
  else
    {
      g_fail++;
    }
}

static void test_base64(void)
{
  const char *in = "aGVsbG8gd29ybGQ=";
  size_t n = 0;
  char *out = base64_decode(in, strlen(in), NULL, &n);
  check("base64_decode", out && n == 11 && !memcmp(out, "hello world", 11));
  free(out);

  char enc[32];
  base64_encode("nuttx", 5, enc, &n);
  enc[n] = '\0';
  check("base64_encode", !strcmp(enc, "bnV0dHg="));

  unsigned char raw[8];
  check("base64url_decode",
        base64url_decode("aGk", raw, sizeof(raw), &n) == 0 && n == 2 &&
        !memcmp(raw, "hi", 2));
}

static void test_urldecode(void)
{
  const char *in = "a%20b+c%2Fd";
  char out[32];
  int n = 0;

  urldecode(in, strlen(in), out, &n);
  check("urldecode", n == 7 && !strcmp(out, "a b c/d"));

  char buf[] = "x%41y+z";
  urlrawdecode(buf);
  check("urlrawdecode", !strcmp(buf, "xAy z"));

  char *d = url_decode("p%3Dq");
  check("url_decode", d && !strcmp(d, "p=q"));
  free(d);
}

static void test_parsehttpurl(void)
{
  char host[64];
  char file[64];
  uint16_t port = 0;
  int ret = netlib_parsehttpurl("http://example.com:8080//a/b?c", &port,
                                host, sizeof(host), file, sizeof(file));
  check("netlib_parsehttpurl", ret == 0 && port == 8080 &&
        !strcmp(host, "example.com") && !strcmp(file, "/a/b?c"));

  char scheme[16];
  struct url_s u;
  memset(&u, 0, sizeof(u));
  u.scheme = scheme;
  u.schemelen = sizeof(scheme);
  u.host = host;
  u.hostlen = sizeof(host);
  u.path = file;
  u.pathlen = sizeof(file);
  ret = netlib_parseurl("https://nuttx.org:443/docs", &u);
  check("netlib_parseurl", ret == 0 && !strcmp(scheme, "https") &&
        !strcmp(host, "nuttx.org") && u.port == 443 &&
        !strcmp(file, "/docs"));
}

static void test_inifile(void)
{
  char path[] = "/tmp/selftest-ini-XXXXXX";
  int fd = mkstemp(path);
  const char *ini = "; comment\n[net]\nip=10.0.0.1\nport=42\n[other]\nip=1\n";

  if (fd < 0 || write(fd, ini, strlen(ini)) != (ssize_t)strlen(ini))
    {
      check("inifile", 0);
      return;
    }

  close(fd);
  INIHANDLE h = inifile_initialize(path);
  char *ip = h ? inifile_read_string(h, "net", "ip", "none") : NULL;
  long port = h ? inifile_read_integer(h, "net", "port", -1) : -1;
  char *missing = h ? inifile_read_string(h, "net", "gw", "dflt") : NULL;
  check("inifile_read_string", ip && !strcmp(ip, "10.0.0.1"));
  check("inifile_read_integer", port == 42);
  check("inifile_default", missing && !strcmp(missing, "dflt"));
  inifile_free_string(ip);
  inifile_free_string(missing);
  inifile_uninitialize(h);
  unlink(path);
}

static void test_minibasic(void)
{
  char buf[256] = "";
  FILE *out = tmpfile();
  FILE *in = fopen("/dev/null", "r");

  basic("10 LET A = 6\n20 PRINT A * 7\n30 PRINT \"N\", LEN(\"abcd\") - 1\n",
        in, out, out);
  rewind(out);
  size_t n = fread(buf, 1, sizeof(buf) - 1, out);
  buf[n] = '\0';
  fclose(out);
  fclose(in);
  check("minibasic", strstr(buf, "42") != NULL && strstr(buf, "N 3") != NULL);
}

static int add(struct xmlrpc_s *call)
{
  int a;
  int b;

  if (xmlrpc_getinteger(call, &a) || xmlrpc_getinteger(call, &b))
    {
      return -1;
    }

  return xmlrpc_buildresponse(call, "{i}", "sum", a + b);
}

static void test_xmlrpc(void)
{
  static struct xmlrpc_entry_s entry = { .func = add, .name = "add" };
  char req[] =
    "<?xml version=\"1.0\"?>\n<methodCall>\n<methodName>add</methodName>\n"
    "<params>\n<param><value><i4>2</i4></value></param>\n"
    "<param><value><int>40</int></value></param>\n</params>\n"
    "</methodCall>\n";
  char resp[4096];
  int sv[2];
  ssize_t n;

  xmlrpc_register(&entry);
  socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
  int ret = xmlrpc_parse(sv[0], req);
  n = recv(sv[1], resp, sizeof(resp) - 1, MSG_DONTWAIT);
  resp[n > 0 ? n : 0] = '\0';
  close(sv[0]);
  close(sv[1]);
  check("xmlrpc", ret == 0 && strstr(resp, "<int>42</int>") != NULL &&
        strstr(resp, "<name>sum</name>") != NULL);
}

/* dhcpc against the dhcpc_env.c fake server, fed the fuzz seeds. */

extern const uint8_t *g_dhcpc_env_data;
extern size_t g_dhcpc_env_size;

static int dhcpc_run_seed(const char *seed, struct dhcpc_state *ds, int *err)
{
  static const uint8_t mac[6] =
  {
    0x02, 0x00, 0x00, 0x00, 0x00, 0x01
  };

  static uint8_t buf[4096];
  char path[512];
  const char *src = getenv("SRC");
  FILE *f;
  void *h;
  int ret;

  snprintf(path, sizeof(path), "%s/mayhem/dhcpc/testsuite/%s",
           src ? src : "/mayhem", seed);
  f = fopen(path, "rb");
  if (f == NULL)
    {
      return -2;
    }

  g_dhcpc_env_size = fread(buf, 1, sizeof(buf), f);
  g_dhcpc_env_data = buf;
  fclose(f);
  h = dhcpc_open("eth0", mac, sizeof(mac));
  if (h == NULL)
    {
      return -2;
    }

  errno = 0;
  ret = dhcpc_request(h, ds);
  *err = errno;
  dhcpc_close(h);
  return ret;
}

static void test_dhcpc(void)
{
  struct dhcpc_state ds;
  int err;
  int ret;

  ret = dhcpc_run_seed("offer_ack.bin", &ds, &err);
  check("dhcpc_request ack",
        ret == 0 && ds.ipaddr.s_addr == inet_addr("192.168.1.50") &&
        ds.netmask.s_addr == inet_addr("255.255.255.0") &&
        ds.default_router.s_addr == inet_addr("192.168.1.1") &&
        ds.serverid.s_addr == inet_addr("192.168.1.1") &&
        ds.num_dnsaddr == 1 &&
        ds.dnsaddr[0].s_addr == inet_addr("8.8.8.8") &&
        ds.lease_time == 3600 && ds.renewal_time == 1800 &&
        ds.rebinding_time == 3150);

  ret = dhcpc_run_seed("offer_nak.bin", &ds, &err);
  check("dhcpc_request nak", ret == -1 && err == ECONNREFUSED);
}

/* dhcpd against the dhcpd_session.c fake clients, fed a fuzz seed:
 * DISCOVER then REQUEST for 10.0.0.2 from 02:00:00:00:00:01. */

extern const uint8_t *g_dhcpd_env_data;
extern size_t g_dhcpd_env_size;
extern int g_dhcpd_env_nsent;
extern uint8_t g_dhcpd_env_sent[];
extern size_t g_dhcpd_env_sentlen;

static void test_dhcpd(void)
{
  static uint8_t buf[4096];
  static const uint8_t ack[3] =
  {
    53, 1, 5
  };

  char path[512];
  const char *src = getenv("SRC");
  FILE *f;
  int ok;

  snprintf(path, sizeof(path), "%s/mayhem/dhcpd/testsuite/%s",
           src ? src : "/mayhem", "discover_request.bin");
  f = fopen(path, "rb");
  if (f == NULL)
    {
      check("dhcpd_run offer+ack", 0);
      return;
    }

  g_dhcpd_env_size = fread(buf, 1, sizeof(buf), f);
  g_dhcpd_env_data = buf;
  g_dhcpd_env_nsent = 0;
  fclose(f);

  ok = dhcpd_run("eth0") == 0 && g_dhcpd_env_nsent == 2 &&
       g_dhcpd_env_sentlen > 240 && g_dhcpd_env_sent[0] == 2 &&
       !memcmp(g_dhcpd_env_sent + 16, "\x0a\x00\x00\x02", 4) &&
       !memcmp(g_dhcpd_env_sent + 28, "\x02\x00\x00\x00\x00\x01", 6) &&
       memmem(g_dhcpd_env_sent + 240, g_dhcpd_env_sentlen - 240,
              ack, sizeof(ack)) != NULL;
  check("dhcpd_run offer+ack", ok);
}

/* ymodem against the ymodem_env.c line, fed the fuzz seed: one file,
 * hello.bin, 1100 bytes of (i * 7) & 255, in a 1K and a 128-byte packet. */

extern const uint8_t *g_ym_env_data;
extern size_t g_ym_env_size;

static size_t g_ym_got;
static int g_ym_same = 1;

static int ym_handler(struct ymodem_ctx_s *ctx)
{
  size_t i;

  if (ctx->packet_type == YMODEM_FILENAME_PACKET)
    {
      return 0;
    }

  for (i = 0; i < ctx->packet_size && g_ym_got < ctx->file_length; i++)
    {
      g_ym_same &= ctx->data[i] == (uint8_t)((g_ym_got++ * 7) & 255);
    }

  return 0;
}

static void test_ymodem(void)
{
  static uint8_t buf[4096];
  struct ymodem_ctx_s ctx;
  char path[512];
  const char *src = getenv("SRC");
  FILE *f;
  int ret;

  snprintf(path, sizeof(path), "%s/mayhem/ymodem/testsuite/session.ym",
           src ? src : "/mayhem");
  f = fopen(path, "rb");
  if (f == NULL)
    {
      check("ymodem_recv", 0);
      return;
    }

  g_ym_env_size = fread(buf, 1, sizeof(buf), f);
  g_ym_env_data = buf;
  fclose(f);
  memset(&ctx, 0, sizeof(ctx));
  ctx.recvfd = -1;
  ctx.sendfd = -1;
  ctx.interval = 15;
  ctx.retry = 100;
  ctx.packet_handler = ym_handler;
  ret = ymodem_recv(&ctx);
  check("ymodem_recv", ret == 0 && !strcmp(ctx.file_name, "hello.bin") &&
        ctx.file_length == 1100 && g_ym_got == 1100 && g_ym_same);
}

int main(void)
{
  test_base64();
  test_urldecode();
  test_parsehttpurl();
  test_inifile();
  test_minibasic();
  test_xmlrpc();
  test_dhcpc();
  test_dhcpd();
  test_ymodem();
  printf("selftest: %d passed, %d failed\n", g_pass, g_fail);
  return g_fail != 0;
}
