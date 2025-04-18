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
    static bool Create(Arena& arena, const char* str, u64 size, Path& path);
    static bool Append(Arena& arena, const Path** paths, u32 pathCount,
                       Path& out);
    static bool Append(Arena& arena, const Path& p1, const Path& p2, Path& out);

    inline operator bool() const { return data_ && size_; }
    inline u64 Size() const { return size_; }

    bool operator==(const Path& rhs);
    bool operator!=(const Path& rhs);

    bool IsRelative() const;
    bool IsAbsolute() const;

    String8 ToString8() const;
    const char* ToCStr() const { return data_; }

private:
    Path(const char* data, u64 size) : data_(data), size_(size) {}
    Path(const Path& path) = delete;

    const char* data_ = nullptr;
    u64 size_ = 0;
};

bool IsValidPathString(String8 string);
bool NormalizePathString(Arena& arena, String8 path, String8& out);

String8 GetParentDirectoryPath(String8 path);

String8 ReadFileToString(Arena& arena, String8 filename, u32 align = 1,
                         bool binaryMode = true);
} // namespace Hls

#endif /* HLS_FILESYSTEM_H */
