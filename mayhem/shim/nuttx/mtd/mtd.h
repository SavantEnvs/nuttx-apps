/* mayhem/shim/nuttx/mtd/mtd.h — host stand-in: only the MTD ioctl numbers
 * nuttx-apps passes to a flash device.  On the host the "device" is a memfd
 * (see mayhem/harnesses/fastboot_env.h), so these ioctls fail with ENOTTY. */
#ifndef __MAYHEM_SHIM_NUTTX_MTD_MTD_H
#define __MAYHEM_SHIM_NUTTX_MTD_MTD_H
#define MTDIOC_BULKERASE 0x4d01
#define MTDIOC_GEOMETRY  0x4d02
#endif
