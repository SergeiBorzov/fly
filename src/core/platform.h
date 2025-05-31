#ifndef FLY_PLATFORM_H
#define FLY_PLATFORM_H

#include <stddef.h>

// clang-format off

// Compiler
#if defined(_MSC_VER)
#    define FLY_PLATFORM_COMPILER_CL
#elif defined(__clang__)
#    define FLY_PLATFORM_COMPILER_CLANG
#elif defined(__GNUC__)
#    define FLY_PLATFORM_COMPILER_GCC
#else
#    error "Unknown compiler"
#endif

// Inline
#if defined(FLY_PLATFORM_COMPILER_CL)
#   define FLY_PLATFORM_INLINE __inline
#else
#   define FLY_PLATFORM_INLINE inline
#endif

#if defined(FLY_PLATFORM_COMPILER_CL)
#   define FLY_PLATFORM_FORCEINLINE __forceinline
#elif __has_attribute(always_inline)
#   define FLY_PLATFORM_FORCEINLINE __attribute__((always_inline))
#else
#   define FLY_PLATFORM_FORCEINLINE FLY_INLINE
#endif

// OS
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#    define FLY_PLATFORM_OS_WINDOWS
#elif __APPLE__
#    include <TargetConditionals.h>
#    if defined TARGET_OS_MAC
#        define FLY_PLATFORM_OS_MAC_OSX
#    else
#        error "Unsupported Apple platform"
#endif
#elif __linux__
#    define FLY_PLATFORM_OS_LINUX
#endif

// POSIX
#if defined(_POSIX_VERSION)
#    define FLY_PLATFORM_POSIX
#endif

// Architecture
#if defined(i386) || defined(__i386) || defined(__i386__) ||                   \
    defined(__i486__) || defined(__i586__) || defined(__i686__) ||             \
    defined(__IA32__) || defined(_M_I86) || defined(_M_IX86) ||                \
    defined(__X86__) || defined(_X86_) || defined(__THW_INTEL__) ||            \
    defined(__I86__) || defined(__INTEL__) || defined(__386)
#    define FLY_PLATFORM_ARCH_X86
#endif

#if defined(__x86_64__) || defined(__x86_64) || defined(__amd64__) ||          \
    defined(__amd64) || defined(_M_X64) || defined(_M_AMD64)
#    define FLY_PLATFORM_ARCH_X86_64
#endif

#if defined(__arm__)
#    define FLY_PLATFORM_ARCH_ARM_32
#endif

#if defined(__aarch64__)
#    define FLY_PLATFORM_ARCH_ARM_64
#endif

// SIMD
#if defined(FLY_PLATFORM_COMPILER_GCC) || defined(FLY_PLATFORM_COMPILER_CLANG) || defined(FLY_PLATFORM_COMPILER_CL)
#    if defined(__AVX512F__)
#        define FLY_PLATFORM_SIMD_AVX512F
#        define FLY_PLATFORM_LANE_WIDTH 16
#    elif defined(__AVX2__)
#        define FLY_PLATFORM_SIMD_AVX2
#        define FLY_PLATFORM_LANE_WIDTH 8
#    elif defined(__SSE__)
#        define FLY_PLATFORM_SIMD_SSE
#        define FLY_PLATFORM_LANE_WIDTH 4
#    else
#        define FLY_PLATFORM_LANE_WIDTH 1
#    endif
#    if defined(__SSE2__)
#        define FLY_PLATFORM_VECTOR_CC __vectorcall
#    else
#        define FLY_PLATFORM_VECTOR_CC
#    endif
#endif
// clang-format on

#endif /* FLY_PLATFORM_H */
