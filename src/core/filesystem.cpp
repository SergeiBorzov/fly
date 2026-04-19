#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "filesystem.h"
#include "memory.h"
#include "thread_context.h"

namespace Fly
{

String8 ReadFileToString(Arena& arena, String8 path, u32 align)
{
    String8 result = String8();

    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);

    const char* mode = "rb";
    const char* pathCStr = String8::PushCStr(scratch, path);
    char* content = nullptr;
    i64 fileSize = 0;

    FILE* file = fopen(pathCStr, mode);
    if (!file)
    {
        goto exit;
    }

    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    content = FLY_PUSH_ARENA_ALIGNED(arena, char, fileSize + 1, align);
    content[fileSize] = '\0';

    if (fread(content, 1, fileSize, file) != static_cast<size_t>(fileSize))
    {
        goto exit;
    }
    result = String8(content, fileSize);

exit:
    if (file)
    {
        fclose(file);
    }
    ArenaPopToMarker(scratch, marker);

    return result;
}

char* ReadFileToCStr(Arena& arena, String8 path, u64& size, u32 align)
{
    char* result = nullptr;

    Arena& scratch = GetScratchArena(&arena);
    ArenaMarker marker = ArenaGetMarker(scratch);

    const char* mode = "rb";
    const char* pathCStr = String8::PushCStr(scratch, path);
    i64 fileSize = 0;

    FILE* file = fopen(pathCStr, mode);
    if (!file)
    {
        goto exit;
    }

    fseek(file, 0, SEEK_END);
    fileSize = ftell(file);
    fseek(file, 0, SEEK_SET);

    result = FLY_PUSH_ARENA_ALIGNED(arena, char, fileSize + 1, align);
    result[fileSize] = '\0';

    if (fread(result, 1, fileSize, file) != static_cast<size_t>(fileSize))
    {
        goto exit;
    }
    size = fileSize;
exit:
    if (file)
    {
        fclose(file);
    }
    ArenaPopToMarker(scratch, marker);

    return result;
}

bool WriteStringToFile(String8 str, String8 path, bool append)
{
    if (!CreateDirectories(path))
    {
        return false;
    }

    bool result = false;
    Arena& scratch = GetScratchArena();
    ArenaMarker marker = ArenaGetMarker(scratch);

    const char* pathCStr = String8::PushCStr(scratch, path);
    const char* mode = append ? "ab" : "wb";
    FILE* file = fopen(pathCStr, mode);
    if (!file)
    {
        goto exit;
    }

    if (fwrite(str.Data(), 1, str.Size(), file) != str.Size())
    {
        goto exit;
    }

    result = true;
exit:
    if (file)
    {
        fclose(file);
    }
    ArenaPopToMarker(scratch, marker);
    return result;
}

} // namespace Fly
