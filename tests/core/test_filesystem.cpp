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

TEST(Path, Create)
{
    InitThreadContext();
    Arena& arena = GetScratchArena();

    String8 a = HLS_STRING8_LITERAL("C:\\Users\\Username\\Documents\\file.txt");
    String8 b = HLS_STRING8_LITERAL("D:/Folder/AnotherFile.txt");
    String8 c = HLS_STRING8_LITERAL("..\\file.txt");
    String8 d = HLS_STRING8_LITERAL("folder\\subfolder\\file.txt");
    String8 e = HLS_STRING8_LITERAL("C:\\folder\file.txt");
    String8 e2 = HLS_STRING8_LITERAL("C:\\.\\.\\.\\folder\\.\\.\\file.txt");
    String8 f = HLS_STRING8_LITERAL("..\\folder\\..\\file.txt");
    String8 g =
        HLS_STRING8_LITERAL("\\\\?\\C:\\Users\\Username\\Documents\\file.txt");
    String8 h = HLS_STRING8_LITERAL("\\\\?\\UNC\\Server\\Share\\file.txt");
    String8 i = HLS_STRING8_LITERAL("");
    String8 k = HLS_STRING8_LITERAL("C:\\folder\\|file.txt");
    String8 l = HLS_STRING8_LITERAL("C:\\??\\InvalidPath");

    Path pA;
    EXPECT_FALSE(pA);
    bool res = Path::Create(arena, a, pA);
    EXPECT_TRUE(res);
    EXPECT_TRUE(pA.IsAbsolute());
    EXPECT_FALSE(pA.IsRelative());
    EXPECT_EQ(pA.ToString8(), a);

    Path pB;
    res = Path::Create(arena, b, pB);
    EXPECT_TRUE(res);
    EXPECT_TRUE(pB.IsAbsolute());
    EXPECT_FALSE(pB.IsRelative());
    EXPECT_EQ(pB.ToString8(), b);

    Path pC;
    res = Path::Create(arena, c, pC);
    EXPECT_TRUE(res);
    EXPECT_TRUE(pC.IsRelative());
    EXPECT_FALSE(pC.IsAbsolute());
    EXPECT_EQ(pC.ToString8(), c);

    Path pD;
    res = Path::Create(arena, d, pD);
    EXPECT_TRUE(res);
    EXPECT_TRUE(pD.IsRelative());
    EXPECT_FALSE(pD.IsAbsolute());
    EXPECT_EQ(pD.ToString8(), d);

    Path pE2;
    res = Path::Create(arena, e2, pE2);
    EXPECT_TRUE(res);
    EXPECT_FALSE(pE2.IsRelative());
    EXPECT_TRUE(pE2.IsAbsolute());
    EXPECT_EQ(pE2.ToString8(), e);

    Path pG;
    res = Path::Create(arena, g, pG);
    EXPECT_TRUE(res);
    EXPECT_TRUE(pG.IsAbsolute());
    EXPECT_FALSE(pG.IsRelative());
    EXPECT_EQ(pG.ToString8(), g);

    Path pH;
    res = Path::Create(arena, h, pH);
    EXPECT_TRUE(res);
    EXPECT_TRUE(pH.IsAbsolute());
    EXPECT_FALSE(pH.IsRelative());
    EXPECT_EQ(pH.ToString8(), h);

    Path pI;
    res = Path::Create(arena, i, pI);
    EXPECT_FALSE(res);

    Path pK;
    res = Path::Create(arena, k, pK);
    EXPECT_FALSE(res);

    Path pL;
    res = Path::Create(arena, l, pL);
    EXPECT_FALSE(res);

    ReleaseThreadContext();
}
