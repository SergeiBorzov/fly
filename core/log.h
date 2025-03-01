#ifndef HLS_LOG_H
#define HLS_LOG_H

#include "types.h"

enum class LogLevel
{
    Debug,
    Warning,
    Info,
    Error,
};

#ifdef NDEBUG
#define HLS_DEBUG_LOG(fmt, ...)
#else
#define HLS_DEBUG_LOG(fmt, ...)                                                \
    LogImpl(LogLevel::Debug, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#endif

#define HLS_WARNING(fmt, ...)                                                  \
    LogImpl(LogLevel::Warning, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define HLS_LOG(fmt, ...)                                                      \
    LogImpl(LogLevel::Info, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define HLS_ERROR(fmt, ...)                                                    \
    LogImpl(LogLevel::Error, __FILE__, __LINE__, fmt, ##__VA_ARGS__)

HlsResult InitLogger(const char* filename = nullptr);
void ShutdownLogger();
i64 LogImpl(LogLevel lvl, const char* file, i32 line, const char* fmt, ...);

#endif /* HLS_LOG_H */
