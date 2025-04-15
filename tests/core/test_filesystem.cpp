#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/core/filesystem.h"

using namespace Hls;

TEST(Path, Validation)
{
#ifdef HLS_PLATFORM_OS_WINDOWS
    String8 a = HLS_STRING8_LITERAL("C:\\Users\\Username\\Documents\\file.txt");
    String8 b = HLS_STRING8_LITERAL("D:/Folder/AnotherFile.txt");
    String8 c = HLS_STRING8_LITERAL("..\\file.txt");
    String8 d = HLS_STRING8_LITERAL("folder\\subfolder\\file.txt");
    String8 e = HLS_STRING8_LITERAL("C:/folder/../file.txt");
    String8 f = HLS_STRING8_LITERAL("..\\folder\\..\\file.txt");
    String8 g =
        HLS_STRING8_LITERAL("\\\\?\\C:\\Users\\Username\\Documents\\file.txt");
    String8 h = HLS_STRING8_LITERAL("\\\\?\\UNC\\Server\\Share\\file.txt");
    String8 i = HLS_STRING8_LITERAL("");
    String8 k = HLS_STRING8_LITERAL("C:\\folder\\|file.txt");
    String8 l = HLS_STRING8_LITERAL("C:\\??\\InvalidPath");

    EXPECT_TRUE(IsValidPathString(a));
    EXPECT_TRUE(IsValidPathString(b));
    EXPECT_TRUE(IsValidPathString(c));
    EXPECT_TRUE(IsValidPathString(d));
    EXPECT_TRUE(IsValidPathString(e));
    EXPECT_TRUE(IsValidPathString(f));
    EXPECT_TRUE(IsValidPathString(g));
    EXPECT_TRUE(IsValidPathString(h));

    EXPECT_FALSE(IsValidPathString(i));
    EXPECT_FALSE(IsValidPathString(k));
    EXPECT_FALSE(IsValidPathString(l));
#endif
}
