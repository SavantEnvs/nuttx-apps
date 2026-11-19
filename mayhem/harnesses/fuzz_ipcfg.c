/* fuzz_ipcfg.c — fsutils/ipcfg text-format reader.
 *
 * The input is an IP configuration file (DEVICE=, IPv4PROTO=, IPv4IPADDR=,
 * IPv6PROTO=, IPv6IPADDR=, ... lines).  It is written where ipcfg_read()
 * looks for the device's file, CONFIG_IPCFG_PATH "/ipcfg-<netdev>", and read
 * back with ipcfg_read() for AF_INET and for AF_INET6.  CONFIG_IPCFG_PATH is
 * /tmp (build.sh); the device name carries the pid so parallel fuzzers never
 * share a file.
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "fsutils/ipcfg.h"

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
  static char netdev[32];
  static char path[sizeof(CONFIG_IPCFG_PATH) + 48];
  struct ipv4cfg_s v4;
  struct ipv6cfg_s v6;
  FILE *f;

  if (!netdev[0])
    {
      snprintf(netdev, sizeof(netdev), "fz%d", (int)getpid());
      snprintf(path, sizeof(path), CONFIG_IPCFG_PATH "/ipcfg-%s", netdev);
    }

  f = fopen(path, "w");
  if (!f)
    {
      return 0;
    }

  fwrite(data, 1, size, f);
  fclose(f);

  memset(&v4, 0, sizeof(v4));
  ipcfg_read(netdev, &v4, AF_INET);
  memset(&v6, 0, sizeof(v6));
  ipcfg_read(netdev, &v6, AF_INET6);
  unlink(path);
  return 0;
}
