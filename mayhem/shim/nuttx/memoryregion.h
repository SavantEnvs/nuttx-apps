/* mayhem/shim/nuttx/memoryregion.h — host stand-in for the board memory
 * region descriptor (CONFIG_BOARD_MEMORY_RANGE is unset on the host). */
#ifndef __MAYHEM_SHIM_NUTTX_MEMORYREGION_H
#define __MAYHEM_SHIM_NUTTX_MEMORYREGION_H
#include <stdint.h>
struct memory_region_s
{
  uintptr_t start;
  uintptr_t end;
  uint32_t flags;
};
#endif
