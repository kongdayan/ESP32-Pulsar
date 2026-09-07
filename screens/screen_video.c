#include "ui.h"
#include "ui_screen.h"

#include <esp_heap_caps.h>
#include <string.h>

#include "sd_card.h"

#include "app_config.h"
#include "video_source.h"
#include "video_layout.h"

static lv_obj_t   *scr          = NULL;
static lv_obj_t   *img          = NULL;
static lv_obj_t   *status_label = NULL;
static lv_timer_t *video_timer  = NULL;

static uint8_t         *frame_buf = NULL;
static lv_img_dsc_t     frame_dsc;
static video_reader_t   reader    = { NULL, NULL, 0u, 0u };
static char             video_path[VIDEO_PATH_MAX];
static bool             playback_ready = false;

static void video_tick(lv_timer_t *timer);

static const ui_timer_binding_t s_video_timer = {
    video_tick, APP_VIDEO_FRAME_MS, &video_timer,
};

static void set_status(video_status_t status)
{
    if (status_label != NULL) lv_label_set_text(status_label, video_status_text(status));
}

static void set_timer_period(video_status_t status)
{
    if (video_timer != NULL) {
        lv_timer_set_period(video_timer, video_timer_period_ms(status,
                                                               APP_VIDEO_FRAME_MS,
                                                               APP_VIDEO_RETRY_MS));
    }
}

static bool open_video(void)
{
    const video_status_t status = video_reader_open(&reader, sd_card_is_mounted(), video_path,
                                                    frame_buf, VIDEO_FRAME_BYTES);
    playback_ready = (status == VIDEO_OK);
    set_status(status);

    if (playback_ready) {
        lv_obj_invalidate(img);
        set_timer_period(VIDEO_OK);
    }
    return playback_ready;
}

static void video_tick(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (!playback_ready) {
        (void)open_video();
        if (!playback_ready) set_timer_period(VIDEO_NO_CARD);
        return;
    }

    const video_status_t status = video_reader_next(&reader);
    if (status == VIDEO_OK) {
        lv_obj_invalidate(img);
        return;
    }

    set_status(status);
    video_reader_close(&reader);
    playback_ready = false;
    set_timer_period(status);
}

static void on_loaded(lv_event_t *e)
{
    LV_UNUSED(e);
    (void)open_video();
}

static void on_unloaded(lv_event_t *e)
{
    LV_UNUSED(e);
    video_reader_close(&reader);
    playback_ready = false;
}

void screen_video_init(void)
{
    (void)video_join_path(video_path, sizeof(video_path), SD_CARD_MOUNT_POINT, VIDEO_FILE_NAME);

    scr = ui_screen_create(NAV_SCREEN_VIDEO);
    ui_screen_set_bg(scr, VIDEO_BG_COLOR);

    lv_obj_add_event_cb(scr, on_loaded, LV_EVENT_SCREEN_LOADED, NULL);
    lv_obj_add_event_cb(scr, on_unloaded, LV_EVENT_SCREEN_UNLOADED, NULL);
    ui_timer_attach(scr, &s_video_timer);

    if (frame_buf == NULL) {
        frame_buf = (uint8_t *)heap_caps_malloc(VIDEO_FRAME_BYTES,
                                                MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (frame_buf == NULL) {
            frame_buf = (uint8_t *)heap_caps_malloc(VIDEO_FRAME_BYTES, MALLOC_CAP_8BIT);
        }
    }

    memset(&frame_dsc, 0, sizeof(frame_dsc));
    frame_dsc.header.always_zero = 0;
    frame_dsc.header.w = VIDEO_FRAME_W;
    frame_dsc.header.h = VIDEO_FRAME_H;
    frame_dsc.header.cf = VIDEO_IMG_COLOR_FMT;
    frame_dsc.data_size = (uint32_t)VIDEO_FRAME_BYTES;
    frame_dsc.data = frame_buf;

    img = lv_img_create(scr);
    lv_obj_set_size(img, VIDEO_FRAME_W, VIDEO_FRAME_H);
    lv_obj_set_pos(img, 0, 0);
    lv_obj_add_flag(img, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
    ui_screen_attach_nav(img, NAV_SCREEN_VIDEO);

    if (frame_buf != NULL) {
        memset(frame_buf, 0, VIDEO_FRAME_BYTES);
        lv_img_set_src(img, &frame_dsc);
    }

    status_label = ui_label_create_aligned(scr,
                                          (frame_buf != NULL) ? VIDEO_STATUS_LOADING : VIDEO_STATUS_NO_MEM,
                                          VIDEO_STATUS_COLOR, &lv_font_montserrat_16,
                                          0, 0, LV_ALIGN_CENTER);

}

lv_obj_t **screen_video_get_ptr(void) { return &scr; }
