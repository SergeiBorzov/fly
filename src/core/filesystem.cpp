#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "memory.h"
#include "platform.h"
#include "thread_context.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#ifdef HLS_PLATFORM_OS_WINDOWS
// clang-format off
#include <windows.h>
#include <pathcch.h>
// clang-format on
#endif

#ifdef HLS_PLATFORM_OS_WINDOWS
#define HLS_PATH_SEPARATOR '\\'
#define HLS_PATH_SEPARATOR_STRING "\\"
#elif defined(HLS_PLATFORM_OS_LINUX) || defined(HLS_PLATFORM_OS_MAC_OSX)
#define HLS_PATH_SEPARATOR '/'
#define HLS_PATH_SEPARATOR_STRING "/"
#else
#error "Not implemented"
#endif

namespace Hls
{

bool IsValidPathString(String8 str)
{
    if (!str)
    {
        return false;
    }

#ifdef HLS_PLATFORM_OS_WINDOWS
    String8 invalidCharacters = HLS_STRING8_LITERAL("<>\"|*");
    for (u64 i = 0; i < str.Size(); i++)
    {
        // No control sequence characters in path
        if (str[i] < 32)
        {
            return false;
        }

        // Colon only allowed after drive letter, e.g C:\...
        if (str[i] == ':')
        {
            if (i == 1 && CharIsAlpha(str[0]))
            {
                continue;
            }
            else if (i == 5 && str[0] == '\\' && str[1] == '\\' &&
                     (str[2] == '?' || str[2] == '.') && str[3] == '\\' &&
                     CharIsAlpha(str[4]))
            {
                continue;
            }
            return false;
        }
        else if (str[i] == '?')
        {
            // Allow only NT file path prefix
            if (i != 2 || str.Size() < 4 || str[0] != '\\' || str[1] != '\\' ||
                str[3] != '\\')
            {
                return false;
            }
        }
        else if (String8::Find(invalidCharacters, str[i]))
        {
            return false;
        }
    }

    if (str.Size() > 1 && (str[0] == '\\' || str[0] == '/') &&
        (str[1] == '\\' || str[1] == '/'))
    {
        if (str[2] == '\\' || str[2] == '/')
        {
            return false;
        }

        bool slashPresent = false;
        bool validCharacterAfterSlash = false;

        for (u64 i = 3; i < str.Size(); i++)
        {
            if (!slashPresent && (str[i] == '\\' || str[i] == '/'))
            {
                slashPresent = true;
            }
            else if (slashPresent && str[i] != '\\' && str[i] != '/')
            {
                validCharacterAfterSlash = true;
                break;
            }
        }

        return slashPresent && validCharacterAfterSlash;
    }

    return true;
#endif
    return false;
}

bool NormalizePathString(Arena& arena, String8 path, String8& out)
{
    if (!IsValidPathString(path))
    {
        return nullptr;
    }
#ifdef HLS_PLATFORM_OS_WINDOWS
    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker scratchMarker = ArenaGetMarker(scratch);

    char* buffer = HLS_ALLOC(scratch, char, path.Size() + 1);
    memcpy(buffer, path.Data(), path.Size());
    for (u64 i = 0; i < path.Size(); i++)
    {
        if (buffer[i] == '/')
        {
            buffer[i] = '\\';
        }
    }
    buffer[path.Size()] = '\0';

    u64 bufferSize = 0;
    for (u64 i = 0; i < path.Size(); i++)
    {
        if (buffer[i] != '\\' || i <= 1 || buffer[i - 1] != '\\')
        {
            buffer[bufferSize++] = buffer[i];
        }
    }
    String8 slashReplacedPath = String8(buffer, bufferSize);

    String8 extendedLengthPrefix = HLS_STRING8_LITERAL("\\\\?\\");
    String8 devicePathPrefix = HLS_STRING8_LITERAL("\\\\.\\");
    if (slashReplacedPath.StartsWith(extendedLengthPrefix) ||
        slashReplacedPath.StartsWith(devicePathPrefix))
    {
        char* normalized = HLS_ALLOC(arena, char, slashReplacedPath.Size() + 1);
        memcpy(normalized, slashReplacedPath.Data(), slashReplacedPath.Size());
        normalized[slashReplacedPath.Size()] = '\0';
        out = String8(normalized, slashReplacedPath.Size());
        ArenaPopToMarker(scratch, scratchMarker);
        return true;
    }

    u64 irremovablePrefixCount = 0;
    if (bufferSize > 2 && CharIsAlpha(buffer[0]) && buffer[1] == ':' &&
        buffer[2] == '\\')
    {
        irremovablePrefixCount = 3;
    }

    if (bufferSize > 1 && buffer[0] == '\\' && buffer[1] == '\\')
    {
        u32 count = 0;
        for (u64 i = 2; i < bufferSize; i++)
        {
            if (slashReplacedPath[i] == '\\')
            {
                count++;
                if (count == 2)
                {
                    irremovablePrefixCount = i;
                    break;
                }
            }
        }
        if (count != 2)
        {
            irremovablePrefixCount = slashReplacedPath.Size();
        }
    }

    u64 segmentStart = irremovablePrefixCount;
    u64 newSize = irremovablePrefixCount;
    bool hasRootSegment = irremovablePrefixCount > 0;
    if (irremovablePrefixCount != slashReplacedPath.Size())
    {
        String8 currentDir = HLS_STRING8_LITERAL(".\\");
        String8 currentDirLast = HLS_STRING8_LITERAL(".");
        String8 parentDir = HLS_STRING8_LITERAL("..\\");
        String8 parentDirLast = HLS_STRING8_LITERAL("..");
        for (u64 i = irremovablePrefixCount; i < bufferSize; i++)
        {
            if (slashReplacedPath[i] == '\\' || i == bufferSize - 1)
            {
                String8 segment(buffer + segmentStart, i - segmentStart + 1);
                if (segment == parentDir || segment == parentDirLast)
                {
                    i64 prevSegmentStart = -1;
                    for (i64 j = static_cast<i64>(newSize) - 2;
                         j >=
                         MAX(static_cast<i64>(irremovablePrefixCount) - 1, 0);
                         j--)
                    {
                        if (buffer[j] == '\\')
                        {
                            prevSegmentStart = j + 1;
                            break;
                        }
                        else if (j == 0)
                        {
                            prevSegmentStart = 0;
                            break;
                        }
                    }

                    if (prevSegmentStart == -1 && !hasRootSegment)
                    {
                        memcpy(buffer + newSize, segment.Data(),
                               segment.Size());
                        newSize += segment.Size();
                    }
                    else if (prevSegmentStart != -1)
                    {
                        String8 prevSegment(buffer + prevSegmentStart,
                                            newSize - prevSegmentStart);
                        if (prevSegment != parentDir)
                        {
                            newSize -= prevSegment.Size();
                        }
                        else if (!hasRootSegment)
                        {
                            memcpy(buffer + newSize, segment.Data(),
                                   segment.Size());
                            newSize += segment.Size();
                        }
                    }
                }
                else if (segment != currentDir ||
                         (segment == currentDirLast && newSize == 0))
                {
                    memcpy(buffer + newSize, segment.Data(), segment.Size());
                    newSize += segment.Size();
                }
                segmentStart = i + 1;
            }
        }
    }

    if (newSize == 0)
    {
        newSize = 1;
        char* normalized = HLS_ALLOC(arena, char, 2);
        normalized[0] = '.';
        normalized[1] = '\0';
        out = String8(normalized, newSize);
        ArenaPopToMarker(scratch, scratchMarker);
        return true;
    }

    char* normalized = HLS_ALLOC(arena, char, newSize + 1);
    memcpy(normalized, buffer, newSize);
    normalized[newSize] = '\0';
    out = String8(normalized, newSize);
    ArenaPopToMarker(scratch, scratchMarker);
    return true;
#endif
    return nullptr;
}

bool Path::Create(Arena& arena, String8 str, Path& path)
{
    if (!IsValidPathString(str))
    {
        return false;
    }

#ifdef HLS_PLATFORM_OS_WINDOWS
    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(arena);
    ArenaMarker scratchMarker = ArenaGetMarker(scratch);

    int sizeNeeded = MultiByteToWideChar(
        CP_UTF8, 0, str.Data(), static_cast<int>(str.Size()), nullptr, 0);
    if (sizeNeeded == 0)
    {
        return false;
    }

    wchar_t* wideInput =
        HLS_ALLOC_ALIGNED(scratch, wchar_t, sizeNeeded + 1, 16);
    Hls::MemZero(wideInput, sizeof(wchar_t) * sizeNeeded + 1);
    if (MultiByteToWideChar(CP_UTF8, 0, str.Data(),
                            static_cast<int>(str.Size()), wideInput,
                            sizeNeeded) == 0)
    {
        ArenaPopToMarker(scratch, scratchMarker);
        return false;
    }
    wideInput[sizeNeeded] = L'\0';
    for (wchar_t* p = wideInput; *p != L'\0'; ++p)
    {
        if (*p == L'/')
        {
            *p = L'\\';
        }
    }

    wchar_t normalized[PATHCCH_MAX_CCH] = {0};
    HRESULT hr = PathCchCanonicalizeEx(
        normalized, PATHCCH_MAX_CCH, L"..\\file.txt", PATHCCH_ALLOW_LONG_PATHS);
    wprintf(L"Wide string: %ls\n", normalized);
    if (FAILED(hr))
    {
        ArenaPopToMarker(scratch, scratchMarker);
        return false;
    }

    sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, normalized, -1, nullptr, 0,
                                     nullptr, nullptr);
    char* data = HLS_ALLOC(arena, char, sizeNeeded);
    if (WideCharToMultiByte(CP_UTF8, 0, normalized, -1, data, sizeNeeded,
                            nullptr, nullptr) == 0)
    {
        ArenaPopToMarker(scratch, scratchMarker);
        ArenaPopToMarker(arena, marker);
        return false;
    }

    path.data_ = data;
    path.size_ = sizeNeeded - 1;

    ArenaPopToMarker(scratch, scratchMarker);
    return true;
#endif
    return false;
}

bool Path::Create(Arena& arena, const char* str, Path& path)
{
    String8 str8 = String8(str, strlen(str));
    return Create(arena, str8, path);
}

String8 Path::ToString8() const { return String8(data_, size_); }

bool Path::IsAbsolute() const
{
    if (!data_ || !size_)
    {
        return false;
    }
#ifdef HLS_PLATFORM_OS_WINDOWS

    if (data_[0] == '\\' && data_[1] == '\\')
    {
        return true;
    }

    if (CharIsAlpha(data_[0]) && data_[1] == ':' &&
        (data_[2] == '\\' || data_[2] == '/'))
    {
        return true;
    }

    return false;
#endif
    return false;
}

bool Path::IsRelative() const
{
    if (!data_ || !size_)
    {
        return false;
    }

    return !IsAbsolute();
}

String8 GetParentDirectoryPath(String8 path)
{
    String8 lastSeparator = String8::FindLast(path, HLS_PATH_SEPARATOR);
    if (!lastSeparator)
    {
        return HLS_STRING8_LITERAL("." HLS_PATH_SEPARATOR_STRING);
    }

    return String8(path.Data(), path.Size() - lastSeparator.Size() + 1);
}

String8 AppendPaths(Arena& arena, String8* paths, u32 pathCount)
{
    u64 totalSize = 0;
    for (u32 i = 0; i < pathCount; i++)
    {
        totalSize += paths[i].Size();
    }

    char* totalData = HLS_ALLOC(arena, char, totalSize);

    u64 offset = 0;
    for (u32 i = 0; i < pathCount; i++)
    {
        memcpy(totalData + offset, paths[i].Data(), paths[i].Size());
        offset += paths[i].Size();
    }

    return String8(totalData, totalSize);
}

String8 ReadFileToString(Arena& arena, String8 filename, u32 align,
                         bool binaryMode)
{
    const char* mode = "rb";
    if (!binaryMode)
    {
        mode = "r";
    }

    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);
    const char* filenameNullTerminated =
        String8::CopyNullTerminate(scratch, filename);

    FILE* file = fopen(filenameNullTerminated, mode);
    if (!file)
    {
        ArenaPopToMarker(scratch, marker);
        return String8();
    }
    ArenaPopToMarker(scratch, marker);

    // Move the file pointer to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    i64 fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // Move back to the beginning of the file

    // Allocate memory for the string
    char* content = HLS_ALLOC_ALIGNED(arena, char, fileSize + 1, align);

    if (!content)
    {
        fclose(file);
        return String8();
    }

    // Read the file into the string
    fread(content, 1, fileSize, file);

    fclose(file);

    return String8(content, fileSize);
}

} // namespace Hls
