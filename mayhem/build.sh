#!/usr/bin/env bash
#
# mayhem/build.sh — build the nuttx-apps fuzz targets and test suites.
#
# nuttx-apps is the application collection of the NuttX RTOS; upstream only
# builds it inside a NuttX kernel tree, into one firmware image with its own
# libc.  For fuzzing, the selected modules are compiled natively on Linux
# against glibc instead:
#   * mayhem/shim/ stands in for the generated NuttX config header
#     (nuttx/config.h, each module's Kconfig defaults) and for the few kernel
#     headers that only make sense on a device (compiler.h, debug.h,
#     boardctl.h, mtd.h, ...).  host.h is force-included in every file.
#   * Every other <nuttx/...> header (list.h, clock.h, ...) comes from the
#     NuttX kernel tree pinned in the image at $NUTTX_SRC (Dockerfile), via
#     -idirafter so glibc's own headers always win over the kernel's libc.
#
# Outputs (in $SRC/mayhem/out): fuzz_<target> (libFuzzer) and
# fuzz_<target>-standalone (run-once reproducer) per libFuzzer target, the
# file-input targets bas and zmodem_rz, plus the test runners nxinit_test (upstream's cmocka
# suite), selftest (known-answer tests), bas_test and zmodem_rz_test (for
# test.sh's known-answer programs and transfers), all built with normal flags.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
: "${NUTTX_SRC:=/opt/toolchains/deps/nuttx-7b4e437/src}"
: "${STANDALONE_FUZZ_MAIN:=/opt/mayhem/StandaloneFuzzTargetMain.c}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"
[ -f "$NUTTX_SRC/include/nuttx/list.h" ] || { echo "missing NuttX kernel headers at $NUTTX_SRC" >&2; exit 1; }
[ -f "$NUTTX_SRC/libs/libc/misc/lib_crc16xmodem.c" ] || { echo "missing NuttX libc CRC sources at $NUTTX_SRC" >&2; exit 1; }

M="$SRC/mayhem"
H="$M/harnesses"
OUT="$M/out"
B="$M/build"
mkdir -p "$OUT" "$B/fuzz" "$B/plain" "$B/test"

INC=(-I"$M/shim" -I"$SRC/include" -idirafter "$NUTTX_SRC/include" -include host.h)

# Coverage instrumentation for the project objects; the fuzzer runtime itself is
# linked only into the libFuzzer binaries.
SANCOV=-fsanitize=fuzzer-no-link

# obj <variant> <src> [extra flags...] -> prints the object path.
#   fuzz:  sanitizers + SanCov + DWARF 3 (linked into fuzz_<t>)
#   plain: sanitizers + DWARF 3, no SanCov (linked into fuzz_<t>-standalone,
#          which must link without libFuzzer, also with SANITIZER_FLAGS empty)
obj() {
  local variant="$1" src="$2"; shift 2
  local o="$B/$variant/$(echo "${src#$SRC/}" | tr '/' '_').o"
  local flags=($SANITIZER_FLAGS $DEBUG_FLAGS)
  [ "$variant" = fuzz ] && flags+=($SANCOV)
  $CC "${INC[@]}" "${flags[@]}" "$@" -c "$src" -o "$o"
  echo "$o"
}

$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$M/lsan_off.cc" -o "$B/lsan_off.o"
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$B/standalone_main.o"

# The NuttX kernel libc CRC routines zmodem and ymodem call, from the pinned
# kernel tree: a dependency, so built uninstrumented, with debug info.
for crc in lib_crc16xmodem lib_crc32; do
  $CC -idirafter "$NUTTX_SRC/include" -DFAR= $DEBUG_FLAGS -c "$NUTTX_SRC/libs/libc/misc/$crc.c" -o "$B/$crc.o"
done

# [HFLAGS=...] target <name> <harness.c> <libs...> ; a lib is "path.c" or
# "path.c|extra flags"; HFLAGS are extra flags for the harness itself.
target() {
  local name="$1" harness="$2"; shift 2
  local fobjs=() pobjs=() spec src extra
  for spec in "$@"; do
    src="${spec%%|*}"; extra=""
    [ "$spec" != "$src" ] && extra="${spec#*|}"
    # shellcheck disable=SC2086
    fobjs+=("$(obj fuzz "$SRC/$src" $extra)")
    # shellcheck disable=SC2086
    pobjs+=("$(obj plain "$SRC/$src" $extra)")
  done
  $CC "${INC[@]}" ${HFLAGS:-} $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$H/$harness" \
    "${fobjs[@]}" "$B/lsan_off.o" -lm -lpthread -o "$OUT/fuzz_$name"
  $CC "${INC[@]}" ${HFLAGS:-} $SANITIZER_FLAGS $DEBUG_FLAGS "$H/$harness" "$B/standalone_main.o" \
    "${pobjs[@]}" "$B/lsan_off.o" -lm -lpthread -o "$OUT/fuzz_$name-standalone"
}

NXINIT="-I$SRC/system/nxinit"
# The harness includes nxinit's private headers (action.h, service.h, ...).
HFLAGS="$NXINIT" target nxinit_parser fuzz_nxinit_parser.c \
  "system/nxinit/parser.c|$NXINIT" "system/nxinit/action.c|$NXINIT" \
  "system/nxinit/service.c|$NXINIT" "system/nxinit/import.c|$NXINIT" \
  "system/nxinit/builtin.c|$NXINIT" "system/nxinit/property_simple.c|$NXINIT"
# fastboot_session.c includes system/fastboot/fastboot.c (main renamed) and
# runs one client session per input on a fresh daemon context.
FB="-I$SRC -include $H/fastboot_env.h -Dmain=fastbootd_main"
target fastboot fuzz_fastboot.c "mayhem/harnesses/fastboot_session.c|$FB" "mayhem/harnesses/fastboot_env.c"
target webclient fuzz_webclient.c \
  "netutils/webclient/webclient.c|-include $H/webclient_env.h" \
  netutils/netlib/netlib_parseurl.c netutils/netlib/netlib_parsehttpurl.c \
  netutils/codecs/base64.c netutils/codecs/urldecode.c
# dhcpc is IPv4-only; like ipcfg, CONFIG_NET_IPv4 is set for this module only.
HFLAGS=-DCONFIG_NET_IPv4=1 target dhcpc fuzz_dhcpc.c \
  "netutils/dhcpc/dhcpc.c|-DCONFIG_NET_IPv4=1 -include $H/dhcpc_env.h" mayhem/harnesses/dhcpc_env.c
# dhcpd_session.c includes netutils/dhcpd/dhcpd.c and its host mocks.
# CONFIG_BUILD_KERNEL drops only dhcpd_start()/dhcpd_stop() (NuttX
# task_create()/kill() daemon management); the harness calls dhcpd_run().
target dhcpd fuzz_dhcpd.c \
  "mayhem/harnesses/dhcpd_session.c|-I$SRC -DCONFIG_NET_IPv4=1 -DCONFIG_BUILD_KERNEL=1 -include $H/dhcpd_env.h"
# ymodem: crc16xmodem() is the kernel libc's (see the CRC objects above).
HFLAGS="-I$SRC $B/lib_crc16xmodem.o" target ymodem fuzz_ymodem.c \
  "system/ymodem/ymodem.c|-include $H/ymodem_env.h" mayhem/harnesses/ymodem_env.c
target codecs fuzz_codecs.c netutils/codecs/base64.c netutils/codecs/urldecode.c
target netlib_url fuzz_netlib_url.c netutils/netlib/netlib_parseurl.c netutils/netlib/netlib_parsehttpurl.c
target xmlrpc fuzz_xmlrpc.c netutils/xmlrpc/xmlparser.c netutils/xmlrpc/response.c
target inifile fuzz_inifile.c fsutils/inifile/inifile.c
target minibasic fuzz_minibasic.c interpreters/minibasic/basic.c
# bas: a file-input target, the interpreter's own main() (bas_main.c) run as
# "bas -r <program>".  It is not an in-process harness because BASIC's
# SYSTEM/BYE statements and the lexer's fatal-error path call exit().
# bas_env.h/.c keep the program's file writes in a per-process scratch dir.
BAS_SRCS=(bas.c bas_auto.c bas_fs.c bas_global.c bas_program.c bas_str.c bas_token.c
  bas_value.c bas_var.c bas_vt100.c bas_main.c)
BAS=(-include nuttx/debug.h -include "$H/bas_env.h")
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$H/bas_env.c" -o "$B/bas_env.o"
$CC "${INC[@]}" "${BAS[@]}" $SANITIZER_FLAGS $DEBUG_FLAGS \
  "${BAS_SRCS[@]/#/interpreters/bas/}" "$B/bas_env.o" "$B/lsan_off.o" -lm -o "$OUT/bas"

# zmodem: a file-input target, the receiver ("rz") built with upstream's own
# host configuration (system/zmodem/host/nuttx/config.h, as Makefile.host
# builds it) and driven by zmodem_rz.c instead of rz_main.c's tty setup.
# That host build has drifted from the sources: zm.h includes
# <nuttx/debug.h>, which host/ lacks (the shim supplies it), and zmodem now
# calls the kernel libc's crc16xmodempart(), which host/crc16.c (crc16part)
# no longer provides.  So the CRCs and their headers come from the pinned
# kernel (the lib_crc16xmodem/lib_crc32 objects built above).
ZM=system/zmodem
ZMFLAGS=(-I"$ZM/host" -I"$SRC/include" -idirafter "$M/shim" -include nuttx/config.h
  -include "$NUTTX_SRC/include/nuttx/crc16.h" -include "$NUTTX_SRC/include/nuttx/crc32.h"
  $SANITIZER_FLAGS $DEBUG_FLAGS)
$CC -D_GNU_SOURCE=1 -I"$ZM/host" -I"$SRC/include" $SANITIZER_FLAGS $DEBUG_FLAGS \
  -c "$H/zmodem_rz.c" -o "$B/zmodem_rz.o"
$CC "${ZMFLAGS[@]}" -include "$H/zmodem_env.h" -c "$ZM/zm_receive.c" -o "$B/zm_receive.o"
for f in zm_state zm_utils; do
  $CC "${ZMFLAGS[@]}" -include "$H/zmodem_io.h" -c "$ZM/$f.c" -o "$B/$f.o"
done
$CC "${ZMFLAGS[@]}" "$B/zmodem_rz.o" "$B"/{zm_receive,zm_state,zm_utils}.o \
  "$ZM"/{zm_proto,zm_watchdog}.c "$B"/{lib_crc16xmodem,lib_crc32}.o \
  "$B/lsan_off.o" -lrt -o "$OUT/zmodem_rz"

# ipcfg's options are per target (not in shim/nuttx/config.h): its files live
# in /tmp on the host, and IPv4/IPv6 are only enabled for this module.
IPCFG='-DCONFIG_NET_IPv4=1 -DCONFIG_NET_IPv6=1 -DCONFIG_IPCFG_PATH="/tmp"'
HFLAGS="$IPCFG" target ipcfg fuzz_ipcfg.c "fsutils/ipcfg/ipcfg.c|$IPCFG" "fsutils/ipcfg/ipcfg_text.c|$IPCFG"

# Test suites, normal flags (no sanitizers): upstream's nxinit cmocka suite,
# and known-answer tests for the other fuzzed modules.
TF=(-O1 $COVERAGE_FLAGS)
$CC "${INC[@]}" "$NXINIT" "${TF[@]}" \
  system/nxinit/{parser,action,service,import,builtin,property_simple}.c \
  system/nxinit/test/*.c -lcmocka -o "$OUT/nxinit_test"
$CC "${INC[@]}" -DCONFIG_NET_IPv4=1 -include "$H/dhcpc_env.h" "${TF[@]}" \
  -c netutils/dhcpc/dhcpc.c -o "$B/test/dhcpc.o"
$CC "${TF[@]}" -c "$H/dhcpc_env.c" -o "$B/test/dhcpc_env.o"
$CC "${INC[@]}" -include "$H/ymodem_env.h" "${TF[@]}" -c system/ymodem/ymodem.c -o "$B/test/ymodem.o"
$CC "${TF[@]}" -c "$H/ymodem_env.c" -o "$B/test/ymodem_env.o"
$CC "${INC[@]}" -I"$SRC" -DCONFIG_NET_IPv4=1 -DCONFIG_BUILD_KERNEL=1 -include "$H/dhcpd_env.h" \
  "${TF[@]}" -c "$H/dhcpd_session.c" -o "$B/test/dhcpd_session.o"
$CC "${INC[@]}" -I"$SRC" -DCONFIG_NET_IPv4=1 "${TF[@]}" "$M/tests/selftest.c" \
  netutils/codecs/base64.c netutils/codecs/urldecode.c \
  netutils/netlib/netlib_parseurl.c netutils/netlib/netlib_parsehttpurl.c \
  fsutils/inifile/inifile.c interpreters/minibasic/basic.c \
  netutils/xmlrpc/xmlparser.c netutils/xmlrpc/response.c \
  "$B/test/dhcpc.o" "$B/test/dhcpc_env.o" "$B/test/dhcpd_session.o" \
  "$B/test/ymodem.o" "$B/test/ymodem_env.o" "$B/lib_crc16xmodem.o" -lm -o "$OUT/selftest"
# zmodem_rz_test: the zmodem receiver with normal flags, same sources and
# configuration as zmodem_rz above.
ZMTEST=(-I"$ZM/host" -I"$SRC/include" -idirafter "$M/shim" -include nuttx/config.h
  -include "$NUTTX_SRC/include/nuttx/crc16.h" -include "$NUTTX_SRC/include/nuttx/crc32.h")
$CC -D_GNU_SOURCE=1 -I"$ZM/host" -I"$SRC/include" "${TF[@]}" -c "$H/zmodem_rz.c" \
  -o "$B/test/zmodem_rz.o"
$CC "${ZMTEST[@]}" "${TF[@]}" -include "$H/zmodem_env.h" -c "$ZM/zm_receive.c" -o "$B/test/zm_receive.o"
for f in zm_state zm_utils; do
  $CC "${ZMTEST[@]}" "${TF[@]}" -include "$H/zmodem_io.h" -c "$ZM/$f.c" -o "$B/test/$f.o"
done
$CC "${ZMTEST[@]}" "${TF[@]}" "$B/test/zmodem_rz.o" "$B/test"/{zm_receive,zm_state,zm_utils}.o \
  "$ZM"/{zm_proto,zm_watchdog}.c "$B"/{lib_crc16xmodem,lib_crc32}.o \
  -lrt -o "$OUT/zmodem_rz_test"
$CC "${TF[@]}" -c "$H/bas_env.c" -o "$B/test/bas_env.o"
$CC "${INC[@]}" "${BAS[@]}" "${TF[@]}" "${BAS_SRCS[@]/#/interpreters/bas/}" "$B/test/bas_env.o" \
  -lm -o "$OUT/bas_test"

ls -1 "$OUT"
