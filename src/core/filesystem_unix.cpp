#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "memory.h"
#include "platform.h"
#include "thread_context.h"

#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define FLY_PATH_SEPARATOR '/'

namespace Fly
{

bool IsValidPathString(String8 str)
{
    if (!str)
    {
        return false;
    }

    for (u64 i = 0; i < str.Size(); i++)
    {
        if (str[i] < 32)
        {
            return false;
        }
    }

    return true;
}

bool NormalizePathString(Arena& arena, String8 path, String8& out)
{
    if (!IsValidPathString(path))
    {
        return false;
    }

    char* normalized = FLY_PUSH_ARENA(arena, char, path.Size() + 1);
    if (!realpath(path.Data(), normalized))
    {
        return false;
    }
    out = String8(normalized, strlen(normalized));
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

    return data_[0] == '/';
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
    if (fread(content, 1, fileSize, file) != static_cast<size_t>(fileSize))
    {
        fclose(file);
        return String8();
    }

    fclose(file);

    return String8(content, fileSize);
}

String8 ReadFileToString(Arena& arena, const Path& path, u32 align)
{
    return ReadFileToString(arena, path.ToCStr(), align);
}

} // namespace Fly
