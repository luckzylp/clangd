// This header forces the use of a particular version of glibc symbols.
// This can make the resulting binary portable to systems with older glibcs.
//
// It must be included in each TU, with CFLAGS="-include glibc_compat.h" etc.
//
// We only list symbols known to be used. This is paired with a test enforcing
// the max glibc symbol version. If the test fails, the list should be extended.
// Find the old alternatives version to target with e.g.:
//   objdump -T /lib/x86_64-linux-gnu/lib{c,m,pthread,rt}.so.* | grep "\bexpf\b"
//
// The version to pin depends on the architecture, so it is a macro:
//   x86_64   GLIBC_2.2.5 (the oldest version glibc ever had)
//   aarch64  GLIBC_2.17  (aarch64 was added in 2.17, nothing older exists)
// The workflow passes -DCOMPAT_SYMBOL_VERSION=GLIBC_2.17 for the arm64 build
// and relies on the default below for x86_64.

#ifndef COMPAT_SYMBOL_VERSION
#define COMPAT_SYMBOL_VERSION GLIBC_2.2.5
#endif

// Two levels are needed: the first expands the macro, the second stringifies.
#define COMPAT_STRINGIFY_2(x) #x
#define COMPAT_STRINGIFY(x) COMPAT_STRINGIFY_2(x)

#define FORCE_SYMBOL_VERSION(sym, version) \
  __asm__(".symver " #sym "," #sym "@" COMPAT_STRINGIFY(version))

FORCE_SYMBOL_VERSION(exp2, COMPAT_SYMBOL_VERSION);
FORCE_SYMBOL_VERSION(expf, COMPAT_SYMBOL_VERSION);
FORCE_SYMBOL_VERSION(log, COMPAT_SYMBOL_VERSION);
FORCE_SYMBOL_VERSION(log2, COMPAT_SYMBOL_VERSION);
FORCE_SYMBOL_VERSION(pow, COMPAT_SYMBOL_VERSION);
FORCE_SYMBOL_VERSION(exp, COMPAT_SYMBOL_VERSION);

#undef FORCE_SYMBOL_VERSION
