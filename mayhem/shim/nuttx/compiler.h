/* mayhem/shim/nuttx/compiler.h — host (glibc/clang) stand-in for the NuttX
 * kernel's include/nuttx/compiler.h, which keys off the NuttX toolchain
 * config.  Only the attribute/storage macros nuttx-apps sources use. */
#ifndef __MAYHEM_SHIM_NUTTX_COMPILER_H
#define __MAYHEM_SHIM_NUTTX_COMPILER_H
#include <nuttx/config.h>
#ifndef FAR
#  define FAR
#endif
#define NEAR
#define DSEG
#define CODE
#define IPTR
#define weak_function        __attribute__((weak))
#define weak_alias(n, a)     extern __typeof(n) a __attribute__((weak, alias(#n)));
#define weak_data            __attribute__((weak))
#define restrict_data
#define noreturn_function    __attribute__((noreturn))
#define packed_struct        __attribute__((packed))
#define begin_packed_struct
#define end_packed_struct    __attribute__((packed))
#define aligned_data(n)      __attribute__((aligned(n)))
#define locate_code(n)
#define locate_data(n)
#define nostackprotect_function
#define naked_function       __attribute__((naked))
#define always_inline_function __attribute__((always_inline, no_instrument_function))
#define inline_function      __attribute__((always_inline)) inline
#define noinline_function    __attribute__((noinline))
#define noinstrument_function __attribute__((no_instrument_function))
#define nooptimiziation_function
#define nosanitize_address
#define nosanitize_undefined
#define unused_code          __attribute__((unused))
#define unused_data          __attribute__((unused))
#define used_code            __attribute__((used))
#define used_data            __attribute__((used))
#define printf_like(a, b)    __attribute__((format(printf, a, b)))
#define sysloglike(a, b)     __attribute__((format(printf, a, b)))
#define scanf_like(a, b)     __attribute__((format(scanf, a, b)))
#define strftime_like(a)     __attribute__((format(strftime, a, 0)))
#define offsetof_member      __builtin_offsetof
#define predict_true(x)      __builtin_expect(!!(x), 1)
#define predict_false(x)     __builtin_expect((x), 0)
#define likely(x)            __builtin_expect(!!(x), 1)
#define unlikely(x)          __builtin_expect(!!(x), 0)
#define return_address(x)    __builtin_return_address(x)
#define UNUSED(a)            ((void)(1 || &(a)))
#define fallthrough          __attribute__((fallthrough))
#define CONFIG_HAVE_LONG_LONG 1
#define CONFIG_HAVE_FLOAT 1
#define CONFIG_HAVE_DOUBLE 1
#define CONFIG_HAVE_LONG_DOUBLE 1
#define CONFIG_HAVE_ANONYMOUS_STRUCT 1
#define CONFIG_HAVE_ANONYMOUS_UNION 1
#endif
