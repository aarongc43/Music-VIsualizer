#include "vendor/unity/src/unity.h"
#include "../src/audio/music_library.h"

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_CREATED_FILES 32

static char g_temp_dir[PATH_MAX];
static char g_created_files[MAX_CREATED_FILES][PATH_MAX];
static size_t g_created_count;

static int make_temp_dir(void) {
    char tmpl[] = "/tmp/bragi_music_lib_XXXXXX";
    if (!mkdtemp(tmpl)) {
        return -1;
    }

    size_t len = strlen(tmpl);
    if (len >= sizeof(g_temp_dir)) {
        return -1;
    }

    memcpy(g_temp_dir, tmpl, len + 1);
    return 0;
}

static int create_file_in_temp(const char *name) {
    if (!name || g_created_count >= MAX_CREATED_FILES) {
        return -1;
    }

    int written = snprintf(g_created_files[g_created_count],
                           sizeof(g_created_files[g_created_count]),
                           "%s/%s",
                           g_temp_dir,
                           name);
    if (written <= 0 || (size_t)written >= sizeof(g_created_files[g_created_count])) {
        return -1;
    }

    FILE *f = fopen(g_created_files[g_created_count], "wb");
    if (!f) {
        return -1;
    }

    fclose(f);
    g_created_count++;
    return 0;
}

void setUp(void) {
    memset(g_temp_dir, 0, sizeof(g_temp_dir));
    memset(g_created_files, 0, sizeof(g_created_files));
    g_created_count = 0;

    TEST_ASSERT_EQUAL_INT(0, make_temp_dir());
}

void tearDown(void) {
    for (size_t i = g_created_count; i > 0; --i) {
        unlink(g_created_files[i - 1]);
    }

    if (g_temp_dir[0] != '\0') {
        rmdir(g_temp_dir);
    }
}

void test_scan_rejects_null_arguments(void) {
    MusicLibrary lib = {0};
    TEST_ASSERT_EQUAL_INT(-1, music_library_scan(NULL, &lib));
    TEST_ASSERT_EQUAL_INT(-1, music_library_scan(g_temp_dir, NULL));
}

void test_scan_empty_directory_returns_no_tracks(void) {
    MusicLibrary lib = {0};

    TEST_ASSERT_EQUAL_INT(0, music_library_scan(g_temp_dir, &lib));
    TEST_ASSERT_EQUAL_UINT64(0u, (unsigned long long)lib.count);
    TEST_ASSERT_EQUAL_UINT64(0u, (unsigned long long)lib.selected_index);
    TEST_ASSERT_EQUAL_UINT64(0u, (unsigned long long)lib.scroll_offset);
    TEST_ASSERT_EQUAL_UINT64((unsigned long long)SIZE_MAX,
                             (unsigned long long)lib.now_playing_index);

    music_library_free(&lib);
}

void test_scan_filters_non_wav_files(void) {
    MusicLibrary lib = {0};

    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("alpha.wav"));
    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("beta.txt"));
    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("gamma.mp3"));
    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("delta.WAV"));

    TEST_ASSERT_EQUAL_INT(0, music_library_scan(g_temp_dir, &lib));
    TEST_ASSERT_EQUAL_UINT64(2u, (unsigned long long)lib.count);
    TEST_ASSERT_TRUE(strcmp(lib.items[0].name, "alpha.wav") == 0 ||
                     strcmp(lib.items[1].name, "alpha.wav") == 0);
    TEST_ASSERT_TRUE(strcmp(lib.items[0].name, "delta.WAV") == 0 ||
                     strcmp(lib.items[1].name, "delta.WAV") == 0);

    music_library_free(&lib);
}

void test_scan_accepts_mixed_case_extensions(void) {
    MusicLibrary lib = {0};

    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("one.WAV"));
    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("two.wAv"));
    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("three.waV"));

    TEST_ASSERT_EQUAL_INT(0, music_library_scan(g_temp_dir, &lib));
    TEST_ASSERT_EQUAL_UINT64(3u, (unsigned long long)lib.count);

    music_library_free(&lib);
}

void test_scan_sorts_case_insensitive(void) {
    MusicLibrary lib = {0};

    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("zebra.wav"));
    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("Alpha.wav"));
    TEST_ASSERT_EQUAL_INT(0, create_file_in_temp("mId.wav"));

    TEST_ASSERT_EQUAL_INT(0, music_library_scan(g_temp_dir, &lib));
    TEST_ASSERT_EQUAL_UINT64(3u, (unsigned long long)lib.count);
    TEST_ASSERT_EQUAL_STRING("Alpha.wav", lib.items[0].name);
    TEST_ASSERT_EQUAL_STRING("mId.wav", lib.items[1].name);
    TEST_ASSERT_EQUAL_STRING("zebra.wav", lib.items[2].name);

    music_library_free(&lib);
}

void test_free_is_safe_for_null_and_zeroed_library(void) {
    MusicLibrary lib = {0};
    music_library_free(NULL);
    music_library_free(&lib);

    TEST_ASSERT_EQUAL_UINT64(0u, (unsigned long long)lib.count);
    TEST_ASSERT_EQUAL_UINT64((unsigned long long)SIZE_MAX,
                             (unsigned long long)lib.now_playing_index);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_scan_rejects_null_arguments);
    RUN_TEST(test_scan_empty_directory_returns_no_tracks);
    RUN_TEST(test_scan_filters_non_wav_files);
    RUN_TEST(test_scan_accepts_mixed_case_extensions);
    RUN_TEST(test_scan_sorts_case_insensitive);
    RUN_TEST(test_free_is_safe_for_null_and_zeroed_library);
    return UNITY_END();
}
