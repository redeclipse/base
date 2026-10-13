/*
* Copyright(c) 2025 Meta Platforms, Inc. and affiliates.
*
* This source code is subject to the terms of the BSD 2 Clause License and
* the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
* was not distributed with this source code in the LICENSE file, you can
* obtain it at https://www.aomedia.org/license/software-license. If the Alliance for Open
* Media Patent License 1.0 was not distributed with this source code in the
* PATENTS file, you can obtain it at https://www.aomedia.org/license/patent-license.
*/

/*
* This file contains configuration macros that control which parts of code are used
* Macros could be fed via command line, so all macros here must check if they are
* already defined!
* All macros must have the following format:
* - all macros must be prefixed with CONFIG_
*/

#ifndef EbConfigMacros_h
#define EbConfigMacros_h

// clang-format off

#ifndef RTC_BUILD
#define RTC_BUILD 0
#endif

#if RTC_BUILD
#define CONFIG_LOG_QUIET                    1

#define CONFIG_ENABLE_QUANT_MATRIX          0
#define CONFIG_ENABLE_OBMC                  0
#define CONFIG_ENABLE_FILM_GRAIN            0
#define CONFIG_ENABLE_HIGH_BIT_DEPTH        0

#define MIN_ENC_PRESET                      ENC_M9
#endif

#ifndef MIN_ENC_PRESET
#define MIN_ENC_PRESET                      ENC_MR
#endif

// When set to 1, EB_CPU_FLAGS_NEON is unconditionally set for all ARCH_AARCH64
// builds, i.e. requiring Neon for library to work. This also allows linker to
// strip code for all C functions which are optimized with Neon SIMD and thus
// reduce final binary size.
// Neon is mandatory in Armv8.0-A (AArch64), which is our minimum Arm target,
// so it is guaranteed for deployment builds, however tests use C functions,
// and hence for unit-test builds (SVT_AV1_UNIT_TEST_BUILD, set by CMake when
// BUILD_TESTING is ON) this must stay at 0.
#if defined(ARCH_AARCH64) && !defined(SVT_AV1_UNIT_TEST_BUILD)
#define CONFIG_ARM_NEON_IS_GUARANTEED       1
#endif

#ifndef CONFIG_ARM_NEON_IS_GUARANTEED
#define CONFIG_ARM_NEON_IS_GUARANTEED       0
#endif

// Same for x86 builds and AVX2 as minimum required SIMD level.
// AVX2 was first released in 2013 on Haswell microarchitecture, all x86
// processors since support it.
// You can set it to 1 to reduce binary size if deployment platforms are
// guaranteed to be not older than Haswell.
#if 0
#define CONFIG_X86_AVX2_IS_GUARANTEED       0
#endif

#ifndef CONFIG_X86_AVX2_IS_GUARANTEED
#define CONFIG_X86_AVX2_IS_GUARANTEED       0
#endif

#ifndef CONFIG_LOG_QUIET
#define CONFIG_LOG_QUIET                    0
#endif

#ifndef CONFIG_ENABLE_QUANT_MATRIX
#define CONFIG_ENABLE_QUANT_MATRIX          1
#endif

#ifndef CONFIG_ENABLE_OBMC
#define CONFIG_ENABLE_OBMC                  1
#endif

#ifndef CONFIG_ENABLE_FILM_GRAIN
#define CONFIG_ENABLE_FILM_GRAIN            1
#endif

#ifndef CONFIG_ENABLE_HIGH_BIT_DEPTH
#define CONFIG_ENABLE_HIGH_BIT_DEPTH        1
#endif

// Fast (non-bit-exact) all-int16 forward transforms for the LBD path: every
// two-product cospi butterfly uses per-product vqrdmulhq_s16 instead of the
// widening multiply. ~3x fewer instructions; tiny rounding error vs the
// bit-exact int16 path. Guarded so a build picks exactly one path.
#ifndef CONFIG_ENABLE_FAST_LBD_TXFM
#define CONFIG_ENABLE_FAST_LBD_TXFM         0
#endif

// Single-thread kernel dispatch: at lp=1, bypass thread creation and run all
// pipeline kernels cooperatively on one thread. Eliminates 15 context switches
// per frame and all inter-stage semaphore/mutex overhead.
#ifndef CONFIG_SINGLE_THREAD_KERNEL
#define CONFIG_SINGLE_THREAD_KERNEL         1
#endif

// Native 8-bit CDEF NEON path (interior blocks in uint8 lanes). ARM-only.
#if defined(ARCH_AARCH64)
#define CDEF_8BITS_PATH 1
#else
#define CDEF_8BITS_PATH 0
#endif

// clang-format on

#endif // EbConfigMacros_h
