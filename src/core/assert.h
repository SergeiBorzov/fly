#ifndef FLY_ASSERT_H
#define FLY_ASSERT_H

#include "platform.h"
#include "types.h"

// clang-format off
#define FLY_STMNT(S) do { S } while (0)

#if defined(FLY_PLATFORM_COMPILER_CL)
#    define FLY_ASSERT_BREAK() __debugbreak()
#elif defined(FLY_PLATFORM_COMPILER_GCC)
#    define FLY_ASSERT_BREAK() __builtin_trap()
#elif defined(FLY_PLATFORM_COMPILER_CLANG)
#    define FLY_ASSERT_BREAK() __builtin_debugtrap()
#elif defined(FLY_PLATFORM_POSIX)
#    include <signal.h>
#    define FLY_ASSERT_BREAK() raise(SIGTRAP)
#else
#    define FLY_ASSERT_BREAK() (*(u32*)0) = 0xDeAdBeEf
#endif  

void AssertImpl(const char* condition, const char* file, i32 line, const char* msg, ...);

#if defined(NDEBUG)
#define FLY_ASSERT(c, ...)
#else
#define FLY_ASSERT(c, ...)                                              \
    FLY_STMNT(if (!(c)) {                                               \
        AssertImpl(#c, __FILE__, __LINE__,                              \
                    #__VA_ARGS__ __VA_OPT__(, )##__VA_ARGS__);          \
    })
#endif

#define FLY_ENSURE(c, ...)                                              \
    FLY_STMNT(if (!(c)) {                                               \
        AssertImpl(#c, __FILE__, __LINE__,                              \
                    #__VA_ARGS__ __VA_OPT__(, )##__VA_ARGS__);          \
    })

// clang-format on

#endif /* FLY_ASSERT_H */
