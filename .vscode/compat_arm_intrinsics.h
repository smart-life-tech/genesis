#pragma once
/**
 * compat_arm_intrinsics.h
 *
 * Compatibility header to fix missing ARM intrinsics when using
 * CMSIS-DSP with the Teensy 4.x (Cortex-M7) toolchain.
 *
 * Safe to include before <arm_math.h> or any CMSIS headers.
 */

#include <stdint.h>

// -----------------------------------------------------------------------------
//  Include CMSIS core for Cortex-M7 if available
// -----------------------------------------------------------------------------
#if defined(ARDUINO_TEENSY41) || defined(__IMXRT1062__)
  #include "core_cm7.h"
#endif

// -----------------------------------------------------------------------------
//  __CLZ – Count Leading Zeros
// -----------------------------------------------------------------------------
#ifndef __CLZ
  #define __CLZ(x) __builtin_clz(x)
#endif

// -----------------------------------------------------------------------------
//  __SSAT – Signed Saturate
// -----------------------------------------------------------------------------
#ifndef __SSAT
  #define __SSAT(ARG1, ARG2) \
    ({ int32_t __res; \
       if ((ARG1) > ((1 << ((ARG2) - 1)) - 1)) __res = ((1 << ((ARG2) - 1)) - 1); \
       else if ((ARG1) < -(1 << ((ARG2) - 1))) __res = -(1 << ((ARG2) - 1)); \
       else __res = (ARG1); \
       __res; })
#endif

// -----------------------------------------------------------------------------
//  __QADD / __QSUB – Saturating Add / Subtract (Q31-style safe versions)
// -----------------------------------------------------------------------------
#ifndef __QADD
  static inline int32_t __QADD(int32_t a, int32_t b) {
    int64_t r = (int64_t)a + (int64_t)b;
    if (r > 0x7FFFFFFF)  r = 0x7FFFFFFF;
    if (r < (int64_t)0x80000000) r = (int64_t)0x80000000;
    return (int32_t)r;
  }
#endif

#ifndef __QSUB
  static inline int32_t __QSUB(int32_t a, int32_t b) {
    int64_t r = (int64_t)a - (int64_t)b;
    if (r > 0x7FFFFFFF)  r = 0x7FFFFFFF;
    if (r < (int64_t)0x80000000) r = (int64_t)0x80000000;
    return (int32_t)r;
  }
#endif

// -----------------------------------------------------------------------------
//  __SMUAD – Dual 16-bit Multiply-Accumulate
// -----------------------------------------------------------------------------
#ifndef __SMUAD
  static inline int32_t __SMUAD(uint32_t x, uint32_t y) {
    int16_t x0 = (int16_t)(x & 0xFFFF);
    int16_t x1 = (int16_t)((x >> 16) & 0xFFFF);
    int16_t y0 = (int16_t)(y & 0xFFFF);
    int16_t y1 = (int16_t)((y >> 16) & 0xFFFF);
    return (int32_t)(x0 * y0 + x1 * y1);
  }
#endif

// -----------------------------------------------------------------------------
//  __SMLALD – 64-bit Multiply-Accumulate (uses __SMUAD)
// -----------------------------------------------------------------------------
#ifndef __SMLALD
  static inline int64_t __SMLALD(uint32_t x, uint32_t y, int64_t acc) {
    return acc + (int64_t)__SMUAD(x, y);
  }
#endif

// -----------------------------------------------------------------------------
//  __REV / __REV16 – Byte order utilities
// -----------------------------------------------------------------------------
#ifndef __REV
  #define __REV(x) __builtin_bswap32(x)
#endif

#ifndef __REV16
  #define __REV16(x) (__builtin_bswap16(x))
#endif

// -----------------------------------------------------------------------------
//  __ROR – Rotate Right
// -----------------------------------------------------------------------------
#ifndef __ROR
  static inline uint32_t __ROR(uint32_t value, uint32_t shift) {
    return (value >> shift) | (value << (32 - shift));
  }
#endif

// -----------------------------------------------------------------------------
//  __BKPT – Debug Breakpoint (no-op for Teensy)
// -----------------------------------------------------------------------------
#ifndef __BKPT
  #define __BKPT(x) __asm__ volatile("nop")
#endif

// -----------------------------------------------------------------------------
//  Sanity marker
// -----------------------------------------------------------------------------
#define COMPAT_ARM_INTRINSICS_LOADED 1
