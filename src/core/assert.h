#ifndef HLS_ASSERT_H
#define HLS_ASSERT_H

#include "platform.h"
#include "types.h"

// clang-format off
#define HLS_STMNT(S) do { S } while (0)

#if defined(HLS_PLATFORM_COMPILER_CL)
#    define HLS_ASSERT_BREAK() __debugbreak()
#elif defined(HLS_PLATFORM_COMPILER_GCC)
#    define HLS_ASSERT_BREAK() __builtin_trap()
#elif defined(HLS_PLATFORM_COMPILER_CLANG)
#    define HLS_ASSERT_BREAK() __builtin_debugtrap()
#elif defined(HLS_PLATFORM_POSIX)
#    include <signal.h>
#    define HLS_ASSERT_BREAK() raise(SIGTRAP)
#else
#    define HLS_ASSERT_BREAK() (*(u32*)0) = 0xDeAdBeEf
#endif  

void AssertImpl(const char* condition, const char* file, i32 line, const char* msg, ...);

#if defined(NDEBUG)
#define HLS_ASSERT(c, ...)
#else
#define HLS_ASSERT(c, ...)                                              \
    HLS_STMNT(if (!(c)) {                                               \
        AssertImpl(#c, __FILE__, __LINE__,                              \
                    #__VA_ARGS__ __VA_OPT__(, )##__VA_ARGS__);          \
    })
#endif

#define HLS_ENSURE(c, ...)                                              \
    HLS_STMNT(if (!(c)) {                                               \
        AssertImpl(#c, __FILE__, __LINE__,                              \
                    #__VA_ARGS__ __VA_OPT__(, )##__VA_ARGS__);          \
    })

// clang-format on

#endif /* HLS_ASSERT_H */
