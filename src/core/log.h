#ifndef FLY_LOG_H
#define FLY_LOG_H

#include "types.h"

enum class LogLevel
{
    Debug,
    Warning,
    Info,
    Error,
};

#ifdef NDEBUG
#define FLY_DEBUG_LOG(fmt, ...)
#else
#define FLY_DEBUG_LOG(fmt, ...)                                                \
    LogImpl(LogLevel::Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif

#define FLY_WARNING(fmt, ...)                                                  \
    LogImpl(LogLevel::Warning, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FLY_LOG(fmt, ...)                                                      \
    LogImpl(LogLevel::Info, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define FLY_ERROR(fmt, ...)                                                    \
    LogImpl(LogLevel::Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

bool InitLogger(const char* filename = nullptr);
void ShutdownLogger();
i64 LogImpl(LogLevel lvl, const char* file, i32 line, const char* fmt, ...);

#endif /* FLY_LOG_H */
