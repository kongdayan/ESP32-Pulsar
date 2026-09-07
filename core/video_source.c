#include "video_source.h"

#include <string.h>

size_t video_frame_bytes(int width, int height, int bytes_per_pixel)
{
    if (width <= 0 || height <= 0 || bytes_per_pixel <= 0) return 0u;
    return (size_t)width * (size_t)height * (size_t)bytes_per_pixel;
}

size_t video_join_path(char *out, size_t out_sz, const char *mount, const char *file_name)
{
    if (out == NULL || out_sz == 0u || mount == NULL || file_name == NULL) return 0u;

    const int written = snprintf(out, out_sz, "%s/%s", mount, file_name);
    if (written < 0 || (size_t)written >= out_sz) {
        out[0] = '\0';
        return 0u;
    }
    return (size_t)written;
}

uint32_t video_frame_count_of(long file_size, size_t frame_bytes)
{
    if (frame_bytes == 0u || file_size <= 0) return VIDEO_NO_FRAMES;
    return (uint32_t)((unsigned long)file_size / (unsigned long)frame_bytes);
}

video_status_t video_reader_open(video_reader_t *r, bool card_mounted, const char *path,
                                uint8_t *buf, size_t frame_bytes)
{
    if (r == NULL) return VIDEO_NO_CARD;

    video_reader_close(r);

    if (!card_mounted) return VIDEO_NO_CARD;
    if (path == NULL || buf == NULL || frame_bytes == 0u) return VIDEO_BAD_FILE;

    r->buf = buf;
    r->frame_bytes = frame_bytes;

    r->fp = fopen(path, "rb");
    if (r->fp == NULL) return VIDEO_NO_FILE;

    r->frames_played = VIDEO_NO_FRAMES;

    if (video_reader_next(r) != VIDEO_OK) {
        fclose(r->fp);
        r->fp = NULL;
        r->buf = NULL;
        r->frame_bytes = 0u;
        return VIDEO_BAD_FILE;
    }

    return VIDEO_OK;
}

video_status_t video_reader_next(video_reader_t *r)
{
    if (r == NULL || r->fp == NULL || r->buf == NULL || r->frame_bytes == 0u) {
        return VIDEO_READ_ERROR;
    }

    const size_t got = fread(r->buf, 1u, r->frame_bytes, r->fp);
    if (got == r->frame_bytes) {
        r->frames_played++;
        return VIDEO_OK;
    }

    /* 循环播放：回到文件头再读一帧 */
    if (fseek(r->fp, 0L, SEEK_SET) != 0) return VIDEO_READ_ERROR;

    const size_t retry = fread(r->buf, 1u, r->frame_bytes, r->fp);
    if (retry != r->frame_bytes) return VIDEO_READ_ERROR;

    r->frames_played++;
    return VIDEO_OK;
}

void video_reader_close(video_reader_t *r)
{
    if (r == NULL) return;

    if (r->fp != NULL) {
        fclose(r->fp);
        r->fp = NULL;
    }
    r->frames_played = VIDEO_NO_FRAMES;
}

bool video_reader_is_ready(const video_reader_t *r)
{
    return r != NULL && r->fp != NULL;
}

const char *video_status_text(video_status_t status)
{
    switch (status) {
    case VIDEO_NO_CARD:    return VIDEO_STATUS_NO_CARD;
    case VIDEO_NO_FILE:    return VIDEO_STATUS_NO_FILE;
    case VIDEO_BAD_FILE:   return VIDEO_STATUS_BAD_FILE;
    case VIDEO_READ_ERROR: return VIDEO_STATUS_READ_ERR;
    case VIDEO_OK:         break;
    }
    return VIDEO_STATUS_IDLE;
}

uint32_t video_timer_period_ms(video_status_t status, uint32_t frame_ms, uint32_t retry_ms)
{
    return (status == VIDEO_OK) ? frame_ms : retry_ms;
}
