/*
 * video_source.h — TF 卡原始 RGB565 视频回放的纯逻辑（路径、帧计算、状态文案）。
 *
 * 只依赖 stdio，主机上可用临时文件完整测试。
 */
#ifndef CORE_VIDEO_SOURCE_H
#define CORE_VIDEO_SOURCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VIDEO_FILE_NAME      "video.rgb"
#define VIDEO_STATUS_NO_CARD "TF card not mounted"
#define VIDEO_STATUS_NO_FILE "Missing /video.rgb"
#define VIDEO_STATUS_BAD_FILE "Invalid video.rgb"
#define VIDEO_STATUS_READ_ERR "Read error"
#define VIDEO_STATUS_LOADING  "Loading video..."
#define VIDEO_STATUS_NO_MEM   "No frame memory"
#define VIDEO_STATUS_IDLE     ""
#define VIDEO_NO_FRAMES       0u

typedef enum {
    VIDEO_OK = 0,
    VIDEO_NO_CARD,     /* TF 卡未挂载 */
    VIDEO_NO_FILE,     /* 找不到 video.rgb */
    VIDEO_BAD_FILE,    /* 文件长度不足一帧 */
    VIDEO_READ_ERROR   /* 播放中读取失败 */
} video_status_t;

typedef struct {
    FILE    *fp;
    uint8_t *buf;
    size_t   frame_bytes;
    uint32_t frames_played;
} video_reader_t;

/* width * height * bytes_per_pixel */
size_t video_frame_bytes(int width, int height, int bytes_per_pixel);

/* 拼接 "<mount>/<file>"，返回写入长度（不含结尾 0）；缓冲区不足时返回 0 */
size_t video_join_path(char *out, size_t out_sz, const char *mount, const char *file_name);

/* 文件可完整播放的帧数 */
uint32_t video_frame_count_of(long file_size, size_t frame_bytes);

/* 打开并把第一帧读进 buf；失败时保持 r->fp == NULL。
 * buf == NULL 或 frame_bytes == 0 视为 VIDEO_BAD_FILE（与旧版一致）。 */
video_status_t video_reader_open(video_reader_t *r, bool card_mounted, const char *path,
                                uint8_t *buf, size_t frame_bytes);

/* 读取下一帧；到文件尾自动回绕继续循环播放 */
video_status_t video_reader_next(video_reader_t *r);

void video_reader_close(video_reader_t *r);
bool video_reader_is_ready(const video_reader_t *r);

const char *video_status_text(video_status_t status);

/* 正常播放用帧周期，异常时退化为重试周期 */
uint32_t video_timer_period_ms(video_status_t status, uint32_t frame_ms, uint32_t retry_ms);

#ifdef __cplusplus
}
#endif

#endif /* CORE_VIDEO_SOURCE_H */
