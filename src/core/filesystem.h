#ifndef FLY_FILESYSTEM_H
#define FLY_FILESYSTEM_H

#include "string8.h"

namespace Fly
{
struct Arena;

struct Path
{
public:
    Path() : data_(nullptr), size_(0) {}

    static bool Create(Arena& arena, String8 str, Path& path);
    static bool Create(Arena& arena, const char* str, u64 size, Path& path);
    static bool Append(Arena& arena, const Path** paths, u32 pathCount,
                       Path& out);
    static bool Append(Arena& arena, const Path& p1, const Path& p2, Path& out);

    inline char operator[](u64 index) const
    {
        FLY_ASSERT(index < size_);
        return data_[index];
    }

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

bool IsValidPathString(String8 str);
bool NormalizePathString(Arena& arena, String8 path, String8& out);

bool GetParentDirectoryPath(Arena& arena, const Path& path, Path& out);

String8 ReadFileToString(Arena& arena, const char* filename, u32 align = 1);
String8 ReadFileToString(Arena& arena, const Path& path, u32 align = 1);
u8* ReadFileToByteArray(const char* filename, u64& size, u32 align = 1);

bool WriteStringToFile(const String8& str, const char* path, bool append = false);

char* ReplaceExtension(const char* filepath, const char* extension);

} // namespace Fly

#endif /* FLY_FILESYSTEM_H */
