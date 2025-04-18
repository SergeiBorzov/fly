#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/core/filesystem.h"
#include "src/core/thread_context.h"

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
    String8 m = HLS_STRING8_LITERAL("//stuff");
    String8 n = HLS_STRING8_LITERAL("//server\\share");

    EXPECT_TRUE(IsValidPathString(a));
    EXPECT_TRUE(IsValidPathString(b));
    EXPECT_TRUE(IsValidPathString(c));
    EXPECT_TRUE(IsValidPathString(d));
    EXPECT_TRUE(IsValidPathString(e));
    EXPECT_TRUE(IsValidPathString(f));
    EXPECT_TRUE(IsValidPathString(g));
    EXPECT_TRUE(IsValidPathString(h));
    EXPECT_TRUE(IsValidPathString(n));

    EXPECT_FALSE(IsValidPathString(i));
    EXPECT_FALSE(IsValidPathString(k));
    EXPECT_FALSE(IsValidPathString(l));
    EXPECT_FALSE(IsValidPathString(m));
#endif
}

TEST(Path, Normalize)
{
    InitThreadContext();
    Arena& arena = GetScratchArena();

    String8 a = HLS_STRING8_LITERAL(
        "C:\\\\\\Users\\Username\\//Documents\\/\\file.txt");
    String8 a2 = HLS_STRING8_LITERAL(
        "C:\\././Users\\.\\.\\.\\Username\\Documents\\.\\file.txt");
    String8 aExpected =
        HLS_STRING8_LITERAL("C:\\Users\\Username\\Documents\\file.txt");
    String8 aActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, a, aActual));
    EXPECT_TRUE(aExpected == aActual);
    String8 a2Actual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, a2, a2Actual));
    EXPECT_TRUE(aExpected == a2Actual);

    String8 b = HLS_STRING8_LITERAL(
        "\\\\?\\C:\\Users\\/Username\\Documents\\/file.txt");
    String8 bExpected =
        HLS_STRING8_LITERAL("\\\\?\\C:\\Users\\Username\\Documents\\file.txt");
    String8 bActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, b, bActual));
    EXPECT_TRUE(bExpected == bActual);

    String8 c = HLS_STRING8_LITERAL("//server\\share");
    String8 cExpected = HLS_STRING8_LITERAL("\\\\server\\share");
    String8 cActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, c, cActual));
    EXPECT_TRUE(cExpected == cActual);

    String8 d = HLS_STRING8_LITERAL("../../../file2.txt");
    String8 dExpected = HLS_STRING8_LITERAL("..\\..\\..\\file2.txt");
    String8 dActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, d, dActual));
    EXPECT_TRUE(dExpected == dActual);

    String8 e = HLS_STRING8_LITERAL("C:\\..\\..\\..\\file3.txt");
    String8 eExpected = HLS_STRING8_LITERAL("C:\\file3.txt");
    String8 eActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, e, eActual));
    EXPECT_TRUE(eExpected == eActual);

    String8 f =
        HLS_STRING8_LITERAL("D:/dir1/dir2/.\\../../dir3/./dir4/../file.txt");
    String8 fExpected = HLS_STRING8_LITERAL("D:\\dir3\\file.txt");
    String8 fActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, f, fActual));
    EXPECT_TRUE(fExpected == fActual);

    String8 g = HLS_STRING8_LITERAL(".");
    String8 gExpected = HLS_STRING8_LITERAL(".");
    String8 gActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, g, gActual));
    EXPECT_TRUE(gExpected == gActual);

    String8 h = HLS_STRING8_LITERAL("..");
    String8 hExpected = HLS_STRING8_LITERAL("..");
    String8 hActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, h, hActual));
    EXPECT_TRUE(hExpected == hActual);

    String8 i = HLS_STRING8_LITERAL("dir1/..");
    String8 iExpected = HLS_STRING8_LITERAL(".");
    String8 iActual;
    EXPECT_TRUE(Hls::NormalizePathString(arena, i, iActual));
    EXPECT_TRUE(iExpected == iActual);

    ReleaseThreadContext();
}

