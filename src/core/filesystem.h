#ifndef HLS_FILESYSTEM_H
#define HLS_FILESYSTEM_H

#include "string8.h"

struct Arena;

namespace Hls
{

struct Path
{
public:
    Path() : data_(nullptr), size_(0) {}

    static bool Create(Arena& arena, String8 str, Path& path);
    static bool Create(Arena& arena, const char* str, Path& path);

    inline operator bool() const { return data_ && size_; }

    bool IsRelative() const;
    bool IsAbsolute() const;

    String8 ToString8() const;
private:
    Path(const char* data, u64 size) : data_(data), size_(size) {}
    Path(const Path& path) = delete;

    const char* data_ = nullptr;
    u64 size_ = 0;
};

bool IsValidPathString(String8 string);

String8 GetParentDirectoryPath(String8 path);
String8 AppendPaths(Arena& arena, String8* paths, u32 count);
String8 ReadFileToString(Arena& arena, String8 filename, u32 align = 1,
                         bool binaryMode = true);
} // namespace Hls

#endif /* HLS_FILESYSTEM_H */
