/* mayhem/shim/host.h — force-included (-include) in every nuttx-apps
 * translation unit built on the host: NuttX's libc extensions that glibc
 * spells differently or lacks. */
#ifndef __MAYHEM_SHIM_HOST_H
#define __MAYHEM_SHIM_HOST_H
#ifndef _GNU_SOURCE
#  define _GNU_SOURCE 1
#endif
#include <nuttx/config.h>
#include <nuttx/compiler.h>
#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/param.h>
#include <pthread.h>
typedef void *(*pthread_startroutine_t)(void *);
/* glibc spells these as casts, which NuttX headers use in #if. */
#include <time.h>
#undef CLOCKS_PER_SEC
#define CLOCKS_PER_SEC 1000000L
#undef CLK_TCK
#define CLK_TCK 100L
#include <inttypes.h>
#ifndef PRIdOFF
#  define PRIdOFF "ld"   /* off_t is long on the x86_64 host */
#endif
/* NuttX's <limits.h> and <netinet/in.h> extras. */
#include <endian.h>
#ifndef CLOCK_MAX
#  define CLOCK_MAX INT64_MAX
#endif
#ifndef HTONS
#  define HTONS htobe16
#  define NTOHS be16toh
#  define HTONL htobe32
#  define NTOHL be32toh
#endif
#ifndef nitems
#  define nitems(a) (sizeof(a) / sizeof((a)[0]))
#endif
#endif
