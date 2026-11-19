/* fuzz_dhcpd.c — netutils/dhcpd, the DHCP server, against fake clients.
 *
 * The input is what clients on the LAN send the server: a sequence of UDP
 * datagrams, each framed as a 2-byte big-endian length and that many bytes
 * (DISCOVER, REQUEST, DECLINE, RELEASE, ... with any options).  The harness
 * calls the daemon's own entry point, dhcpd_run("eth0"), which reads each
 * datagram, parses its options (dhcpd_parseoptions(), including the
 * file/sname overload fields) and answers it from the lease table; when the
 * input is used up the recv() mock stops it (dhcpd_session.c).
 *
 * Per input everything is fresh: dhcpd_run() allocates and zeroes the
 * server state (leases included) and frees it on return.  The pool
 * configuration (g_dhcpd_config) is only changed by dhcpd_set_*(), which
 * the harness never calls.
 */

#include <stdint.h>
#include <stddef.h>

#include "netutils/dhcpd.h"

extern const uint8_t *g_dhcpd_env_data;
extern size_t g_dhcpd_env_size;
extern int g_dhcpd_env_nsent;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  g_dhcpd_env_data = data;
  g_dhcpd_env_size = size;
  g_dhcpd_env_nsent = 0;
  dhcpd_run("eth0");
  return 0;
}
