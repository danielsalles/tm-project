#include "test_framework.h"

#include "platform/Platform.h"

#include <cstdio>
#include <sys/stat.h>
#ifdef _WIN32
  #include <direct.h>
#endif

TEST(platform, ticks_monotonic) {
    uint32_t a = tmx::GetTicks();
    uint32_t b = tmx::GetTicks();
    EXPECT_TRUE(b >= a);
}

TEST(platform, performance_counter) {
    uint64_t f = tmx::GetPerformanceFrequency();
    EXPECT_TRUE(f > 0);
    uint64_t a = tmx::GetPerformanceCounter();
    uint64_t b = tmx::GetPerformanceCounter();
    EXPECT_TRUE(b >= a);
}

TEST(platform, local_time_sane) {
    int y, mo, d, h, mi, s;
    tmx::GetLocalTime(y, mo, d, h, mi, s);
    EXPECT_TRUE(y >= 2024);
    EXPECT_TRUE(mo >= 1 && mo <= 12);
    EXPECT_TRUE(h >= 0 && h < 24);
}

TEST(platform, file_roundtrip) {
    const char* path = "tm_test_tmp.txt";
    FILE* f = fopen(path, "wb");
    EXPECT_TRUE(f != nullptr);
    fputs("wyd", f);
    fclose(f);

    EXPECT_TRUE(tmx::FileExists(path));
    EXPECT_EQ(tmx::FileSize(path), 3);

    f = tmx::OpenAsset(path, "rb");
    EXPECT_TRUE(f != nullptr);
    if (f) {
        char buf[8] = {};
        fread(buf, 1, 3, f);
        fclose(f);
        EXPECT_EQ(buf[0], 'w');
        EXPECT_EQ(buf[2], 'd');
    }
    remove(path);
    EXPECT_FALSE(tmx::FileExists(path));
}

TEST(platform, backslash_path_normalization) {
#ifdef _WIN32
    _mkdir("tm_test_dir");
#else
    mkdir("tm_test_dir", 0755);
#endif
    const char* path = "tm_test_dir\\tm_test_tmp2.txt";
    FILE* f = tmx::OpenAsset(path, "wb");
    EXPECT_TRUE(f != nullptr);
    if (f)
        fclose(f);
    EXPECT_TRUE(tmx::FileExists("tm_test_dir/tm_test_tmp2.txt"));
    remove("tm_test_dir/tm_test_tmp2.txt");
    remove("tm_test_dir");
}
