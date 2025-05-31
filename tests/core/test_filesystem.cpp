#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/core/filesystem.h"
#include "src/core/thread_context.h"

using namespace Fly;

TEST(Path, Validation)
{
#ifdef FLY_PLATFORM_OS_WINDOWS
    String8 a = FLY_STRING8_LITERAL("C:\\Users\\Username\\Documents\\file.txt");
    String8 b = FLY_STRING8_LITERAL("D:/Folder/AnotherFile.txt");
    String8 c = FLY_STRING8_LITERAL("..\\file.txt");
    String8 d = FLY_STRING8_LITERAL("folder\\subfolder\\file.txt");
    String8 e = FLY_STRING8_LITERAL("C:/folder/../file.txt");
    String8 f = FLY_STRING8_LITERAL("..\\folder\\..\\file.txt");
    String8 g =
        FLY_STRING8_LITERAL("\\\\?\\C:\\Users\\Username\\Documents\\file.txt");
    String8 h = FLY_STRING8_LITERAL("\\\\?\\UNC\\Server\\Share\\file.txt");
    String8 i = FLY_STRING8_LITERAL("");
    String8 k = FLY_STRING8_LITERAL("C:\\folder\\|file.txt");
    String8 l = FLY_STRING8_LITERAL("C:\\??\\InvalidPath");
    String8 m = FLY_STRING8_LITERAL("//stuff");
    String8 n = FLY_STRING8_LITERAL("//server\\share");

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

    String8 a = FLY_STRING8_LITERAL(
        "C:\\\\\\Users\\Username\\//Documents\\/\\file.txt");
    String8 a2 = FLY_STRING8_LITERAL(
        "C:\\././Users\\.\\.\\.\\Username\\Documents\\.\\file.txt");
    String8 aExpected =
        FLY_STRING8_LITERAL("C:\\Users\\Username\\Documents\\file.txt");
    String8 aActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, a, aActual));
    EXPECT_TRUE(aExpected == aActual);
    String8 a2Actual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, a2, a2Actual));
    EXPECT_TRUE(aExpected == a2Actual);

    String8 b = FLY_STRING8_LITERAL(
        "\\\\?\\C:\\Users\\/Username\\Documents\\/file.txt");
    String8 bExpected =
        FLY_STRING8_LITERAL("\\\\?\\C:\\Users\\Username\\Documents\\file.txt");
    String8 bActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, b, bActual));
    EXPECT_TRUE(bExpected == bActual);

    String8 c = FLY_STRING8_LITERAL("//server\\share");
    String8 cExpected = FLY_STRING8_LITERAL("\\\\server\\share");
    String8 cActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, c, cActual));
    EXPECT_TRUE(cExpected == cActual);

    String8 d = FLY_STRING8_LITERAL("../../../file2.txt");
    String8 dExpected = FLY_STRING8_LITERAL("..\\..\\..\\file2.txt");
    String8 dActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, d, dActual));
    EXPECT_TRUE(dExpected == dActual);

    String8 e = FLY_STRING8_LITERAL("C:\\..\\..\\..\\file3.txt");
    String8 eExpected = FLY_STRING8_LITERAL("C:\\file3.txt");
    String8 eActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, e, eActual));
    EXPECT_TRUE(eExpected == eActual);

    String8 f =
        FLY_STRING8_LITERAL("D:/dir1/dir2/.\\../../dir3/./dir4/../file.txt");
    String8 fExpected = FLY_STRING8_LITERAL("D:\\dir3\\file.txt");
    String8 fActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, f, fActual));
    EXPECT_TRUE(fExpected == fActual);

    String8 g = FLY_STRING8_LITERAL(".");
    String8 gExpected = FLY_STRING8_LITERAL(".");
    String8 gActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, g, gActual));
    EXPECT_TRUE(gExpected == gActual);

    String8 h = FLY_STRING8_LITERAL("..");
    String8 hExpected = FLY_STRING8_LITERAL("..");
    String8 hActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, h, hActual));
    EXPECT_TRUE(hExpected == hActual);

    String8 i = FLY_STRING8_LITERAL("dir1/..");
    String8 iExpected = FLY_STRING8_LITERAL(".");
    String8 iActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, i, iActual));
    EXPECT_TRUE(iExpected == iActual);

    String8 j = FLY_STRING8_LITERAL("/file.txt");
    String8 jExpected = FLY_STRING8_LITERAL("\\file.txt");
    String8 jActual;
    EXPECT_TRUE(Fly::NormalizePathString(arena, j, jActual));
    EXPECT_TRUE(jExpected == jActual);

    ReleaseThreadContext();
}

TEST(Path, Append)
{
    InitThreadContext();
    Arena& arena = GetScratchArena();

    Path a;
    EXPECT_TRUE(Fly::Path::Create(arena, FLY_STRING8_LITERAL("."), a));
    Path aa;
    EXPECT_TRUE(Fly::Path::Append(arena, a, a, aa));
    printf("%s vs %s\n", a.ToCStr(), aa.ToCStr());
    EXPECT_TRUE(a == aa);

    Path b;
    EXPECT_TRUE(Fly::Path::Create(arena, FLY_STRING8_LITERAL("dir1/dir2"), b));
    Path c;
    EXPECT_TRUE(Fly::Path::Create(arena, FLY_STRING8_LITERAL("..\\..\\.."), c));
    Path d;
    EXPECT_TRUE(Fly::Path::Create(arena, FLY_STRING8_LITERAL("../.."), d));
    Path bcExpected;
    EXPECT_TRUE(
        Fly::Path::Create(arena, FLY_STRING8_LITERAL(".."), bcExpected));
    Path bcActual;
    EXPECT_TRUE(Fly::Path::Append(arena, b, c, bcActual));
    EXPECT_TRUE(bcExpected == bcActual);
    Path bdActual;
    EXPECT_TRUE(Fly::Path::Append(arena, b, d, bdActual));
    EXPECT_TRUE(a == bdActual);

    Path e;
    EXPECT_TRUE(
        Fly::Path::Create(arena, FLY_STRING8_LITERAL("D:/dir1/dir2/.\\"), e));
    Path f;
    EXPECT_TRUE(Fly::Path::Create(
        arena, FLY_STRING8_LITERAL("../../dir3/./dir4/../file.txt"), f));
    Path efExpected;
    EXPECT_TRUE(Fly::Path::Create(
        arena, FLY_STRING8_LITERAL("D:\\dir3\\file.txt"), efExpected));
    Path efActual;
    EXPECT_TRUE(Fly::Path::Append(arena, e, f, efActual));
    EXPECT_TRUE(efExpected == efActual);

    ReleaseThreadContext();
}
