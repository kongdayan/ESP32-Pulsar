#include "minitest.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "video_source.h"

#define TEST_FILE "build/test_video.rgb"
#define FRAME_W   4
#define FRAME_H   2
#define FRAME_BPP 2
#define FRAME_SZ  ((size_t)(FRAME_W * FRAME_H * FRAME_BPP))

static void write_frames(int count)
{
    FILE *fp = fopen(TEST_FILE, "wb");
    CHECK(fp != NULL);
    if (fp == NULL) return;

    for (int f = 0; f < count; f++) {
        for (size_t i = 0; i < FRAME_SZ; i++) {
            fputc((int)((f * 61u + i) & 0xFFu), fp);
        }
    }
    fclose(fp);
}

static void remove_test_file(void)
{
    (void)remove(TEST_FILE);
}

MT_TEST(test_video_frame_bytes)
{
    CHECK_EQ(video_frame_bytes(360, 360, 2), 360 * 360 * 2);
    CHECK_EQ(video_frame_bytes(FRAME_W, FRAME_H, FRAME_BPP), FRAME_SZ);
    CHECK_EQ(video_frame_bytes(0, 360, 2), 0u);
    CHECK_EQ(video_frame_bytes(360, 360, 0), 0u);
    CHECK_EQ(video_frame_bytes(-1, 10, 2), 0u);
}

MT_TEST(test_video_join_path)
{
    char buf[64];

    CHECK_EQ(video_join_path(buf, sizeof(buf), "/sdcard", "video.rgb"),
             strlen("/sdcard/video.rgb"));
    CHECK_STR_EQ(buf, "/sdcard/video.rgb");

    CHECK_EQ(video_join_path(buf, sizeof(buf), "", "x"), 2u);
    CHECK_STR_EQ(buf, "/x");

    /* 缓冲区不够时不得越界写 */
    CHECK_EQ(video_join_path(buf, 4u, "/sdcard", "video.rgb"), 0u);
    CHECK_EQ(buf[0], '\0');

    CHECK_EQ(video_join_path(NULL, sizeof(buf), "/m", "f"), 0u);
    CHECK_EQ(video_join_path(buf, 0u, "/m", "f"), 0u);
    CHECK_EQ(video_join_path(buf, sizeof(buf), NULL, "f"), 0u);
    CHECK_EQ(video_join_path(buf, sizeof(buf), "/m", NULL), 0u);
}

MT_TEST(test_video_frame_count_of)
{
    CHECK_EQ(video_frame_count_of(2L * (long)FRAME_SZ, FRAME_SZ), 2u);
    CHECK_EQ(video_frame_count_of(2L * (long)FRAME_SZ + 1L, FRAME_SZ), 2u); /* 尾帧不完整不算 */
    CHECK_EQ(video_frame_count_of(0L, FRAME_SZ), 0u);
    CHECK_EQ(video_frame_count_of(-5L, FRAME_SZ), 0u);
    CHECK_EQ(video_frame_count_of(100L, 0u), 0u);
}

MT_TEST(test_video_status_text)
{
    CHECK_STR_EQ(video_status_text(VIDEO_OK), VIDEO_STATUS_IDLE);
    CHECK_STR_EQ(video_status_text(VIDEO_NO_CARD), "TF card not mounted");
    CHECK_STR_EQ(video_status_text(VIDEO_NO_FILE), "Missing /video.rgb");
    CHECK_STR_EQ(video_status_text(VIDEO_BAD_FILE), "Invalid video.rgb");
    CHECK_STR_EQ(video_status_text(VIDEO_READ_ERROR), "Read error");
    /* 非法枚举值也要落到"无提示"而不是野指针 */
    CHECK_STR_EQ(video_status_text((video_status_t)99), VIDEO_STATUS_IDLE);
}

MT_TEST(test_video_timer_period)
{
    CHECK_EQ(video_timer_period_ms(VIDEO_OK, 42u, 500u), 42u);
    CHECK_EQ(video_timer_period_ms(VIDEO_NO_CARD, 42u, 500u), 500u);
    CHECK_EQ(video_timer_period_ms(VIDEO_READ_ERROR, 42u, 500u), 500u);
}

MT_TEST(test_video_open_requires_card)
{
    video_reader_t r = { NULL, NULL, 0u, 0u };
    uint8_t buf[FRAME_SZ];
    write_frames(2);

    CHECK_EQ(video_reader_open(&r, false, TEST_FILE, buf, FRAME_SZ), VIDEO_NO_CARD);
    CHECK_FALSE(video_reader_is_ready(&r));

    /* 空指针/参数缺失 */
    CHECK_EQ(video_reader_open(NULL, true, TEST_FILE, buf, FRAME_SZ), VIDEO_NO_CARD);
    CHECK_EQ(video_reader_open(&r, true, NULL, buf, FRAME_SZ), VIDEO_BAD_FILE);
    CHECK_EQ(video_reader_open(&r, true, TEST_FILE, NULL, FRAME_SZ), VIDEO_BAD_FILE);
    CHECK_EQ(video_reader_open(&r, true, TEST_FILE, buf, 0u), VIDEO_BAD_FILE);

    remove_test_file();
}

MT_TEST(test_video_open_missing_file)
{
    video_reader_t r = { NULL, NULL, 0u, 0u };
    uint8_t buf[FRAME_SZ];

    remove_test_file();
    CHECK_EQ(video_reader_open(&r, true, TEST_FILE, buf, FRAME_SZ), VIDEO_NO_FILE);
    CHECK_FALSE(video_reader_is_ready(&r));
}

MT_TEST(test_video_open_truncated_file)
{
    video_reader_t r = { NULL, NULL, 0u, 0u };
    uint8_t buf[FRAME_SZ];

    FILE *fp = fopen(TEST_FILE, "wb");
    CHECK(fp != NULL);
    if (fp != NULL) {
        for (size_t i = 0; i < FRAME_SZ - 1u; i++) fputc(0x5A, fp);
        fclose(fp);
    }

    CHECK_EQ(video_reader_open(&r, true, TEST_FILE, buf, FRAME_SZ), VIDEO_BAD_FILE);
    CHECK_FALSE(video_reader_is_ready(&r));
    remove_test_file();
}

MT_TEST(test_video_play_and_loop)
{
    video_reader_t r = { NULL, NULL, 0u, 0u };
    uint8_t buf[FRAME_SZ];

    write_frames(2);
    CHECK_EQ(video_reader_open(&r, true, TEST_FILE, buf, FRAME_SZ), VIDEO_OK);
    CHECK(video_reader_is_ready(&r));
    CHECK_EQ(r.frames_played, 1u);
    CHECK_EQ(buf[0], 0u); /* 第 0 帧首字节 */

    CHECK_EQ(video_reader_next(&r), VIDEO_OK);
    CHECK_EQ(r.frames_played, 2u);
    CHECK_EQ(buf[0], (uint8_t)(61u & 0xFFu)); /* 第 1 帧首字节 */

    /* 第 3 次读取越过文件尾：自动回绕到第 0 帧继续播放 */
    CHECK_EQ(video_reader_next(&r), VIDEO_OK);
    CHECK_EQ(buf[0], 0u);

    video_reader_close(&r);
    CHECK_FALSE(video_reader_is_ready(&r));
    CHECK_EQ(r.frames_played, 0u);

    /* 关闭之后再读只会得到错误 */
    CHECK_EQ(video_reader_next(&r), VIDEO_READ_ERROR);
    CHECK_EQ(video_reader_next(NULL), VIDEO_READ_ERROR);
    remove_test_file();
}

MT_TEST(test_video_reopen_replaces_previous_handle)
{
    video_reader_t r = { NULL, NULL, 0u, 0u };
    uint8_t buf[FRAME_SZ];

    write_frames(1);
    CHECK_EQ(video_reader_open(&r, true, TEST_FILE, buf, FRAME_SZ), VIDEO_OK);
    /* 再次 open 不应泄漏/复用旧句柄 */
    CHECK_EQ(video_reader_open(&r, true, TEST_FILE, buf, FRAME_SZ), VIDEO_OK);
    CHECK(video_reader_is_ready(&r));
    CHECK_EQ(r.frames_played, 1u);

    video_reader_close(&r);
    video_reader_close(&r);   /* 幂等 */
    video_reader_close(NULL);
    remove_test_file();
}

MT_TEST(test_video_mount_point_composes_with_file_name)
{
    char path[64];
    const size_t n = video_join_path(path, sizeof(path), "/sdcard", VIDEO_FILE_NAME);

    CHECK(n > 0u);
    CHECK_STR_EQ(path, "/sdcard/video.rgb");
}
