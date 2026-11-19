/* mayhem/shim/nuttx/debug.h — host stand-in for the kernel's debug macros,
 * as a NuttX build with CONFIG_DEBUG_FEATURES off sees them: every
 * subsystem log macro compiles away. */
#ifndef __MAYHEM_SHIM_NUTTX_DEBUG_H
#define __MAYHEM_SHIM_NUTTX_DEBUG_H
#include <nuttx/config.h>
#define _none(...)
#define _alert(...)
#define _err(...)
#define _warn(...)
#define _info(...)
#define alert(...)
#define err(...)
#define warn(...)
#define info(...)
#define DEBUGASSERT(f)
#define DEBUGVERIFY(f) ((void)(f))
#define DEBUGPANIC()
#define ninfo(...)
#define nerr(...)
#define nwarn(...)
#define finfo(...)
#define ferr(...)
#define fwarn(...)
#define syslog_info(...)
#endif
