/*
 * screen_balance.c — DeepSeek 账户余额屏。
 *
 * 数据由电脑端经 BLE 写入 core/balance_model 的共享存储；本屏每秒检查一次，
 * 只在数据/链路状态变化时才重设文本（避免整屏无谓重绘）。数据缺失或超过
 * APP_BLE_STALE_MS 未更新时显示 "Waiting for BLE"。
 */
#include "ui.h"
#include "ui_screen.h"

#include <stdio.h>

#include "app_config.h"
#include "balance_layout.h"
#include "balance_model.h"
#include "ui_theme.h"

static lv_obj_t *scr = NULL;
static lv_obj_t *amount_label = NULL;
static lv_obj_t *currency_label = NULL;
static lv_obj_t *status_label = NULL;
static lv_obj_t *row_labels[BAL_ROW_COUNT] = { NULL, NULL, NULL };
static lv_timer_t *refresh_timer = NULL;
static uint32_t last_rx_ms = 0;
static bool last_live = false;

static uint32_t role_color(ui_color_role_t role)
{
    return ui_theme_color(UI_THEME_DARK, role);
}

static void set_row(int index, const char *name, const char *amount)
{
    if (row_labels[index] == NULL) return;
    char text[BAL_ROW_TEXT_MAX];
    snprintf(text, sizeof(text), BAL_FMT_ROW, name, amount);
    lv_label_set_text(row_labels[index], text);
}

static void update_labels(void)
{
    const uint32_t now_ms = (uint32_t)lv_tick_get();
    balance_data_t data;
    const bool live = balance_store_get(&data) && data.valid &&
                      !balance_is_stale(&data, now_ms, APP_BLE_STALE_MS);

    /* 数据没变就不动标签：lv_label_set_text 每次都会 invalidate 整个对象 */
    if (live == last_live && (!live || data.rx_ms == last_rx_ms)) return;
    last_live = live;
    last_rx_ms = live ? data.rx_ms : 0u;

    if (amount_label == NULL || currency_label == NULL || status_label == NULL) return;

    if (!live) {
        lv_label_set_text(amount_label, BAL_TEXT_NO_AMOUNT);
        lv_label_set_text(currency_label, APP_TEXT_WAITING_BLE);
        lv_label_set_text(status_label, "");
        for (int i = 0; i < BAL_ROW_COUNT; i++) {
            if (row_labels[i] != NULL) lv_label_set_text(row_labels[i], "");
        }
        return;
    }

    char total[BALANCE_AMOUNT_TEXT_MAX];
    char granted[BALANCE_AMOUNT_TEXT_MAX];
    char topped[BALANCE_AMOUNT_TEXT_MAX];
    balance_format_amount(data.total_cents, total, sizeof(total));
    balance_format_amount(data.granted_cents, granted, sizeof(granted));
    balance_format_amount(data.topped_up_cents, topped, sizeof(topped));

    lv_label_set_text(amount_label, total);
    lv_label_set_text(currency_label,
                      data.currency[0] != '\0' ? data.currency : BAL_TEXT_NO_AMOUNT);
    lv_label_set_text(status_label,
                      data.is_available ? BAL_TEXT_AVAILABLE : BAL_TEXT_UNAVAILABLE);
    lv_obj_set_style_text_color(
        status_label,
        lv_color_hex(role_color(data.is_available ? UI_ROLE_TEXT_GOOD : UI_ROLE_TEXT_ACCENT)),
        LV_PART_MAIN | LV_STATE_DEFAULT);

    set_row(0, BAL_TEXT_TOTAL, total);
    set_row(1, BAL_TEXT_TOPPED, topped);
    set_row(2, BAL_TEXT_GRANTED, granted);
}

static void on_refresh(lv_timer_t *timer)
{
    (void)timer;
    update_labels();
}

static const ui_timer_binding_t k_refresh_binding = {
    .cb = on_refresh,
    .period_ms = APP_BLE_REFRESH_MS,
    .handle = &refresh_timer,
    .user_data = NULL,
};

void screen_balance_init(void)
{
    scr = ui_screen_create(NAV_SCREEN_BALANCE);
    ui_screen_set_bg(scr, role_color(UI_ROLE_BG));

    /* 屏幕可能被删除后重建：句柄必须归零，否则 ui_timer_loaded_cb 不会重建定时器 */
    refresh_timer = NULL;
    last_rx_ms = 0u;
    last_live = false;

    (void)ui_label_create_aligned(scr, BAL_TEXT_TITLE, role_color(UI_ROLE_TEXT_ACCENT),
                                  BAL_FONT_TITLE, 0, BAL_TITLE_Y, LV_ALIGN_CENTER);
    amount_label = ui_label_create_aligned(scr, BAL_TEXT_NO_AMOUNT,
                                           role_color(UI_ROLE_TEXT_PRIMARY),
                                           BAL_FONT_AMOUNT, 0, BAL_AMOUNT_Y, LV_ALIGN_CENTER);
    currency_label = ui_label_create_aligned(scr, APP_TEXT_WAITING_BLE, role_color(UI_ROLE_TICK),
                                             BAL_FONT_CURRENCY, 0, BAL_CURRENCY_Y, LV_ALIGN_CENTER);
    status_label = ui_label_create_aligned(scr, "", role_color(UI_ROLE_TEXT_GOOD),
                                           BAL_FONT_ROW, 0, BAL_STATUS_Y, LV_ALIGN_CENTER);

    row_labels[0] = ui_label_create_aligned(scr, "", role_color(UI_ROLE_TEXT_PRIMARY),
                                            BAL_FONT_ROW, 0, BAL_ROW1_Y, LV_ALIGN_CENTER);
    row_labels[1] = ui_label_create_aligned(scr, "", role_color(UI_ROLE_TICK),
                                            BAL_FONT_ROW, 0, BAL_ROW2_Y, LV_ALIGN_CENTER);
    row_labels[2] = ui_label_create_aligned(scr, "", role_color(UI_ROLE_TICK),
                                            BAL_FONT_ROW, 0, BAL_ROW3_Y, LV_ALIGN_CENTER);

    update_labels();
    ui_timer_attach(scr, &k_refresh_binding);
}

lv_obj_t **screen_balance_get_ptr(void) { return &scr; }
