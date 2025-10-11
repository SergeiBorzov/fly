#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "memory.h"
#include "platform.h"
#include "thread_context.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#include <windows.h>

#define FLY_PATH_SEPARATOR '\\'

namespace Fly
{

bool IsValidPathString(String8 str)
{
    if (!str)
    {
        return false;
    }

    String8 invalidCharacters = FLY_STRING8_LITERAL("<>\"|*");
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
}

bool NormalizePathString(Arena& arena, String8 path, String8& out)
{
    if (!IsValidPathString(path))
    {
        return false;
    }

    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker scratchMarker = ArenaGetMarker(scratch);

    char* buffer = FLY_PUSH_ARENA(scratch, char, path.Size() + 1);
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

    String8 extendedLengthPrefix = FLY_STRING8_LITERAL("\\\\?\\");
    String8 devicePathPrefix = FLY_STRING8_LITERAL("\\\\.\\");
    if (slashReplacedPath.StartsWith(extendedLengthPrefix) ||
        slashReplacedPath.StartsWith(devicePathPrefix))
    {
        char* normalized =
            FLY_PUSH_ARENA(arena, char, slashReplacedPath.Size() + 1);
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
        String8 currentDir = FLY_STRING8_LITERAL(".\\");
        String8 currentDirLast = FLY_STRING8_LITERAL(".");
        String8 parentDir = FLY_STRING8_LITERAL("..\\");
        String8 parentDirLast = FLY_STRING8_LITERAL("..");
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
        char* normalized = FLY_PUSH_ARENA(arena, char, 2);
        normalized[0] = '.';
        normalized[1] = '\0';
        out = String8(normalized, newSize);
        ArenaPopToMarker(scratch, scratchMarker);
        return true;
    }

    char* normalized = FLY_PUSH_ARENA(arena, char, newSize + 1);
    memcpy(normalized, buffer, newSize);
    normalized[newSize] = '\0';
    out = String8(normalized, newSize);
    ArenaPopToMarker(scratch, scratchMarker);
    return true;
}

bool Path::Create(Arena& arena, String8 str, Path& path)
{
    String8 out;
    if (!NormalizePathString(arena, str, out))
    {
        return false;
    }

    path.data_ = out.Data();
    path.size_ = out.Size();
    return true;
}

bool Path::Create(Arena& arena, const char* str, u64 size, Path& path)
{
    String8 str8 = String8(str, size);
    return Create(arena, str8, path);
}

bool Path::operator==(const Path& rhs)
{
    return size_ == rhs.size_ && (!size_ || !memcmp(data_, rhs.data_, size_));
}

bool Path::operator!=(const Path& rhs) { return !(*this == rhs); }

bool Path::Append(Arena& arena, const Path** paths, u32 pathCount, Path& out)
{
    if (pathCount <= 1)
    {
        return false;
    }

    if (!paths[0] || !(*paths[0]))
    {
        return false;
    }
    for (u64 i = 1; i < pathCount; i++)
    {
        if (!paths[i] || !(*paths[i]) || paths[i]->IsAbsolute())
        {
            return false;
        }
    }

    u64 totalSize = 0;
    for (u32 i = 0; i < pathCount; i++)
    {
        totalSize += paths[i]->Size();
    }

    if (totalSize == 0)
    {
        out = Path();
        return true;
    }

    totalSize += pathCount - 1;

    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker scratchMarker = ArenaGetMarker(scratch);

    char* totalData = FLY_PUSH_ARENA(scratch, char, totalSize);
    u64 offset = 0;
    for (u32 i = 0; i < pathCount; i++)
    {
        if (paths[i]->Size() > 0)
        {
            memcpy(totalData + offset, paths[i]->ToCStr(), paths[i]->Size());
            offset += paths[i]->Size();
            if (i != pathCount - 1)
            {
                totalData[offset++] = FLY_PATH_SEPARATOR;
            }
        }
    }

    String8 pathStr;
    if (!NormalizePathString(arena, String8(totalData, totalSize), pathStr))
    {
        ArenaPopToMarker(scratch, scratchMarker);
        return false;
    }

    out.data_ = pathStr.Data();
    out.size_ = pathStr.Size();

    return true;
}

bool Path::Append(Arena& arena, const Path& p1, const Path& p2, Path& out)
{
    const Path* paths[2] = {&p1, &p2};
    return Append(arena, paths, 2, out);
}

String8 Path::ToString8() const { return String8(data_, size_); }

bool Path::IsAbsolute() const
{
    if (!data_ || !size_)
    {
        return false;
    }

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
}

bool Path::IsRelative() const
{
    if (!data_ || !size_)
    {
        return false;
    }

    return !IsAbsolute();
}

bool GetParentDirectoryPath(Arena& arena, const Path& path, Path& out)
{
    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);

    Path parentDir;
    Path::Create(scratch, FLY_STRING8_LITERAL(".."), parentDir);

    bool res = Path::Append(arena, path, parentDir, out);
    ArenaPopToMarker(scratch, marker);
    return res;
}

String8 ReadFileToString(Arena& arena, const char* path, u32 align)
{
    const char* mode = "rb";

    FILE* file = fopen(path, mode);
    if (!file)
    {
        return String8();
    }

    // Move the file pointer to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    i64 fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // Move back to the beginning of the file

    // Allocate memory for the string
    char* content = FLY_PUSH_ARENA_ALIGNED(arena, char, fileSize + 1, align);

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

u8* ReadFileToByteArray(const char* path, u64& size, u32 align)
{
    const char* mode = "rb";

    FILE* file = fopen(path, mode);
    if (!file)
    {
        return nullptr;
    }

    // Move the file pointer to the end of the file to determine its size
    fseek(file, 0, SEEK_END);
    i64 fileSize = ftell(file);
    fseek(file, 0, SEEK_SET); // Move back to the beginning of the file

    // Allocate memory for the string
    u8* content =
        static_cast<u8*>(Fly::AllocAligned(sizeof(u8) * (fileSize + 1), align));
    content[fileSize] = '\0';

    if (!content)
    {
        fclose(file);
        return nullptr;
    }

    // Read the file into the string
    if (fread(content, 1, fileSize, file) != static_cast<size_t>(fileSize))
    {
        fclose(file);
        Fly::Free(content);
        return nullptr;
    }

    fclose(file);

    size = fileSize;
    return content;
}

String8 ReadFileToString(Arena& arena, const Path& path, u32 align)
{
    return ReadFileToString(arena, path.ToCStr(), align);
}

static bool CreateDirectories(const char* path)
{
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp));
    tmp[sizeof(tmp) - 1] = '\0';

    for (char* p = tmp + 1; *p; p++)
    {
        if (*p == '/' || *p == '\\')
        {
            *p = '\0';
            if (!CreateDirectoryA(tmp, nullptr))
            {
                DWORD err = GetLastError();
                if (err != ERROR_ALREADY_EXISTS)
                {
                    return false;
                }
            }
            *p = '\\';
        }
    }

    return true;
}

bool WriteStringToFile(const String8& str, const char* path, bool append)
{
    if (!CreateDirectories(path))
    {
        return false;
    }

    const char* mode = append ? "ab" : "wb";
    FILE* f = fopen(path, mode);
    if (!f)
    {
        return false;
    }

    if (fwrite(str.Data(), 1, str.Size(), f) != str.Size())
    {
        fclose(f);
        return false;
    }

    fclose(f);
    return true;
}

char* ReplaceExtension(const char* filepath, const char* extension)
{
    if (!filepath || !extension)
    {
        return nullptr;
    }

    const char* lastBackslash = strrchr(filepath, '\\');
    const char* lastSlash = strrchr(filepath, '/');
    const char* lastDot = strrchr(filepath, '.');

    const char* lastDelimeter = nullptr;
    if (lastBackslash && lastSlash)
    {
        ptrdiff_t diff = lastBackslash - lastSlash;
        if (diff > 0)
        {
            lastDelimeter = lastBackslash;
        }
        else
        {
            lastDelimeter = lastSlash;
        }
    }
    else if (lastBackslash)
    {
        lastDelimeter = lastBackslash;
    }
    else if (lastSlash)
    {
        lastDelimeter = lastSlash;
    }

    char* result = nullptr;
    if (lastDot && (!lastDelimeter || lastDot > lastDelimeter))
    {
        size_t len = (lastDot - filepath) + strlen(extension) + 1;
        result = static_cast<char*>(malloc(len * sizeof(char)));
        strncpy(result, filepath, lastDot - filepath);
        result[lastDot - filepath] = '\0';
        strcat(result, extension);
    }
    else
    {
        size_t len = strlen(filepath) + strlen(extension) + 1;
        result = static_cast<char*>(malloc(len * sizeof(char)));
        strcpy(result, filepath);
        strcat(result, extension);
    }
    return result;
}

} // namespace Fly
