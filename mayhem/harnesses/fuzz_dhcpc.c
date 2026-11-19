/* fuzz_dhcpc.c — netutils/dhcpc, the DHCP client, against a fake server.
 *
 * The input is what a DHCP server (or anyone on the LAN) sends the client:
 * a sequence of UDP datagrams, each framed as a 2-byte big-endian length and
 * that many bytes (dhcpc_env.c).  The harness runs the client the way
 * netinit and the dhcpc example do: dhcpc_open() for interface "eth0" with
 * MAC 02:00:00:00:00:01, then dhcpc_request(), which sends DISCOVER, parses
 * OFFERs, sends REQUEST and parses the ACK/NAK (dhcpc_parsemsg(),
 * dhcpc_parseoptions()), then dhcpc_close().
 *
 * A reply is only accepted if it echoes the client's transaction ID,
 * 00 00 00 01 (dhcpc_env.h), and its MAC; the seeds do.  Per input
 * everything is fresh: the client state is allocated by dhcpc_open(), and
 * the interface address the mocks keep is reset.
 */

#include <netinet/in.h>
#include <stdint.h>
#include <string.h>

#include "netutils/dhcpc.h"

extern const uint8_t *g_dhcpc_env_data;
extern size_t g_dhcpc_env_size;
extern struct in_addr g_dhcpc_env_ifaddr;

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  static const uint8_t mac[6] =
  {
    0x02, 0x00, 0x00, 0x00, 0x00, 0x01
  };

  struct dhcpc_state ds;
  void *handle;

  g_dhcpc_env_data = data;
  g_dhcpc_env_size = size;
  memset(&g_dhcpc_env_ifaddr, 0, sizeof(g_dhcpc_env_ifaddr));

  handle = dhcpc_open("eth0", mac, sizeof(mac));
  if (handle != NULL)
    {
      dhcpc_request(handle, &ds);
      dhcpc_close(handle);
    }

  return 0;
}
