/*
 * video_layout.h — TF 卡视频回放屏的视图常量。
 */
#ifndef SCREENS_VIDEO_LAYOUT_H
#define SCREENS_VIDEO_LAYOUT_H

#include "app_config.h"
#include "video_source.h"

/* 素材分辨率固定为整屏，像素格式 RGB565 */
#define VIDEO_FRAME_W          APP_SCREEN_PX
#define VIDEO_FRAME_H          APP_SCREEN_PX
#define VIDEO_FRAME_BPP        APP_BYTES_PER_PIXEL
#define VIDEO_FRAME_BYTES      video_frame_bytes(VIDEO_FRAME_W, VIDEO_FRAME_H, VIDEO_FRAME_BPP)
#define VIDEO_IMG_COLOR_FMT    LV_IMG_CF_TRUE_COLOR

/* 拼出来的完整路径缓冲区 */
#define VIDEO_PATH_MAX         64

#define VIDEO_IMG_BORDER_WIDTH 0

/* 底色纯黑，状态提示为白色 16px */
#define VIDEO_BG_COLOR         0x000000u
#define VIDEO_STATUS_COLOR     0xFFFFFFu

#endif /* SCREENS_VIDEO_LAYOUT_H */
