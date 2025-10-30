#pragma once
/*
  Minimal cmsis_compiler.h shim for Teensy + CMSIS-DSP builds.

  This provides the minimum compiler macros/types that arm_math_types.h expects,
  while avoiding including CMSIS's cmsis_gcc.h (which redefines IRQ macros and
  conflicts with the Teensy core.)
*/

#ifndef __CMSIS_COMPILER_H
#define __CMSIS_COMPILER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>

/* assembly / inline helpers */
#ifndef __ASM
  #if defined(__GNUC__)
    #define __ASM __asm__
  #else
    #define __ASM __asm
  #endif
#endif

#ifndef __INLINE
  #define __INLINE inline
#endif

#ifndef __STATIC_INLINE
  #define __STATIC_INLINE static inline
#endif

#ifndef __STATIC_FORCEINLINE
  #if defined(__GNUC__)
    #define __STATIC_FORCEINLINE static inline __attribute__((always_inline))
  #else
    #define __STATIC_FORCEINLINE static inline
  #endif
#endif

#ifndef __NO_RETURN
  #if defined(__cplusplus)
    #define __NO_RETURN _Noreturn
  #else
    #define __NO_RETURN _Noreturn
  #endif
#endif

/* weak / packed attributes (common CMSIS macros) */
#ifndef __WEAK
  #if defined(__GNUC__)
    #define __WEAK __attribute__((weak))
  #else
    #define __WEAK
  #endif
#endif

#ifndef __PACKED
  #if defined(__GNUC__)
    #define __PACKED __attribute__((__packed__))
  #else
    #define __PACKED
  #endif
#endif

#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
#define CONST_CAST_COEFFS(x) const_cast<float*>(x)
#else
#define CONST_CAST_COEFFS(x) (float*)(x)
#endif

#endif /* __CMSIS_COMPILER_H */