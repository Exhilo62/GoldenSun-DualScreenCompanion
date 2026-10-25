// Force-included when the shared core is built with Visual C++: swallows GCC-only syntax and supplies the GCC builtins the sources use, so they need no changes.
#ifndef MSVC_COMPAT_H
#define MSVC_COMPAT_H
#include <intrin.h>
#define __attribute__(x)
static __inline int __builtin_popcount(unsigned v) { return (int) __popcnt(v); }
static __inline int __builtin_clz(unsigned v) { unsigned long i; _BitScanReverse(&i, v); return 31 - (int) i; }
#endif
