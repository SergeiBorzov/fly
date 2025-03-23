#include <stdarg.h>
#include <stdio.h>
#include <time.h>

#include "assert.h"
#include "log.h"

static struct
{
    FILE* stream = nullptr;
} sLogger;

bool InitLogger(const char* filename)
{
    // Reinit logger
    if (sLogger.stream != nullptr && sLogger.stream != stdout)
    {
        fclose(sLogger.stream);
    }

    if (filename != nullptr)
    {
        sLogger.stream = fopen(filename, "a");
        if (sLogger.stream == nullptr)
        {
            fprintf(stderr, "Hls logger failed to open file: %s", filename);
            return false;
        }
    }
    else
    {
        sLogger.stream = stdout;
    }

    return true;
}

void ShutdownLogger()
{
    if (sLogger.stream)
    {
        fclose(sLogger.stream);
    }
}

static const char* LogLevelToString(LogLevel lvl)
{
    switch (lvl)
    {
        case LogLevel::Debug:
        {
            return "Debug";
        }
        case LogLevel::Warning:
        {
            return "Warn";
        }
        case LogLevel::Info:
        {
            return "Info";
        }
        case LogLevel::Error:
        {
            return "Error";
        }
        default:
        {
            return "";
        }
    }
}

#define TIMESTAMP_MSG_SIZE 20
static bool WriteTimeStamp(const struct tm* timeInfo, char* buffer, u64 size)
{
    HLS_ASSERT(buffer);
    HLS_ASSERT(size >= TIMESTAMP_MSG_SIZE);

    i32 written =
        sprintf(buffer, "%02d/%02d/%02d %02d:%02d:%02d", timeInfo->tm_mday,
                timeInfo->tm_mon + 1, timeInfo->tm_year + 1900,
                timeInfo->tm_hour, timeInfo->tm_min, timeInfo->tm_sec);
    return written == TIMESTAMP_MSG_SIZE - 1; // excluding null terminator
}

static i64 FormattedMsg(FILE* fp, const char* timestamp, const char* lvlString,
                        const char* file, i32 line, const char* fmt,
                        va_list args)
{
    i64 size = 0;
    i64 totalSize = 0;

    if ((size = fprintf(fp, "[%s][%s][%s:%d]: ", timestamp, lvlString, file,
                        line)) > 0)
    {
        totalSize += size;
    }
    if ((size = vfprintf(fp, fmt, args)) > 0)
    {
        totalSize += size;
    }
    if ((size = fprintf(fp, "\n")) > 0)
    {
        totalSize += size;
    }

    return totalSize;
}

i64 LogImpl(LogLevel lvl, const char* file, i32 line, const char* fmt, ...)
{
    if (sLogger.stream == nullptr)
    {
        return -1;
    }

    time_t rawTime;
    struct tm* timeInfo;
    time(&rawTime);
    timeInfo = localtime(&rawTime);

    char timestamp[TIMESTAMP_MSG_SIZE];
    if (!WriteTimeStamp(timeInfo, timestamp, TIMESTAMP_MSG_SIZE))
    {
        return -2;
    }

    va_list args;
    va_start(args, fmt);
    i64 size = FormattedMsg(sLogger.stream, timestamp, LogLevelToString(lvl),
                            file, line, fmt, args);
    va_end(args);

    return size;
}
