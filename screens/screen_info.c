#include "ui.h"
#include "ui_screen.h"

#include "info_layout.h"

static lv_obj_t *scr = NULL;

static void build_tab_online(lv_obj_t *tab)
{
    lv_obj_t *spinner = lv_spinner_create(tab, INFO_SPINNER_ANIM_MS, INFO_SPINNER_ARC_DEG);
    lv_obj_set_size(spinner, INFO_SPINNER_PX, INFO_SPINNER_PX);
    lv_obj_set_align(spinner, LV_ALIGN_CENTER);
    lv_obj_clear_flag(spinner, LV_OBJ_FLAG_CLICKABLE);
}

static void build_tab_calendar(lv_obj_t *tab)
{
    lv_obj_t *cal = lv_calendar_create(tab);
    lv_calendar_set_today_date(cal, INFO_CAL_YEAR, INFO_CAL_MONTH, INFO_CAL_DAY);
    lv_calendar_set_showed_date(cal, INFO_CAL_YEAR, INFO_CAL_MONTH);
    lv_calendar_header_arrow_create(cal);
    lv_obj_set_size(cal, INFO_CAL_W, INFO_CAL_H);
    lv_obj_set_pos(cal, INFO_CAL_OFFSET_X, INFO_CAL_OFFSET_Y);
    lv_obj_set_align(cal, LV_ALIGN_CENTER);
}

static void build_tab_setting(lv_obj_t *tab)
{
    /* Wi-Fi 行 */
    (void)ui_label_create_aligned(tab, INFO_LABEL_WIFI, INFO_TEXT_COLOR_DEFAULT, NULL,
                                  INFO_ROW_LABEL_X, INFO_ROW_WIFI_Y, LV_ALIGN_CENTER);
    lv_obj_t *sw_wifi = lv_switch_create(tab);
    lv_obj_set_size(sw_wifi, INFO_ROW_SWITCH_W, INFO_ROW_SWITCH_H);
    lv_obj_set_pos(sw_wifi, INFO_ROW_SWITCH_X, INFO_ROW_WIFI_Y);
    lv_obj_set_align(sw_wifi, LV_ALIGN_CENTER);

    /* Bluetooth 行 */
    (void)ui_label_create_aligned(tab, INFO_LABEL_BLUETOOTH, INFO_TEXT_COLOR_DEFAULT, NULL,
                                  INFO_ROW_LABEL_X, INFO_ROW_BT_LABEL_Y, LV_ALIGN_CENTER);
    lv_obj_t *sw_bt = lv_switch_create(tab);
    lv_obj_set_size(sw_bt, INFO_ROW_SWITCH_W, INFO_ROW_SWITCH_H);
    lv_obj_set_pos(sw_bt, INFO_ROW_SWITCH_X, INFO_ROW_BT_SWITCH_Y);
    lv_obj_set_align(sw_bt, LV_ALIGN_CENTER);

    /* 音量滑条 */
    (void)ui_label_create_aligned(tab, INFO_LABEL_VOLUME, INFO_TEXT_COLOR_DEFAULT, NULL,
                                  INFO_ROW_VOL_LABEL_X, INFO_ROW_VOL_LABEL_Y, LV_ALIGN_CENTER);
    lv_obj_t *slider = lv_slider_create(tab);
    lv_slider_set_value(slider, INFO_SLIDER_START_VALUE, LV_ANIM_OFF);
    lv_obj_set_size(slider, INFO_ROW_SLIDER_W, INFO_ROW_SLIDER_H);
    lv_obj_set_pos(slider, INFO_ROW_SLIDER_X, INFO_ROW_SLIDER_Y);
    lv_obj_set_align(slider, LV_ALIGN_CENTER);

    /* 自动扫描勾选框 */
    lv_obj_t *chk = lv_checkbox_create(tab);
    lv_checkbox_set_text(chk, INFO_CHECK_AUTOSCAN);
    lv_obj_set_pos(chk, INFO_ROW_CHECK_X, INFO_ROW_CHECK_Y);
    lv_obj_set_align(chk, LV_ALIGN_CENTER);
}

void screen_info_init(void)
{
    scr = ui_screen_create(NAV_SCREEN_INFO);

    lv_obj_t *tabview = lv_tabview_create(scr, INFO_TAB_SIDE, INFO_TAB_BAR_PX);
    lv_obj_set_size(tabview, INFO_TABVIEW_W, INFO_TABVIEW_H);
    lv_obj_set_align(tabview, LV_ALIGN_CENTER);
    lv_obj_clear_flag(tabview, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_text_font(tabview, &lv_font_montserrat_10, LV_PART_MAIN | LV_STATE_DEFAULT);

    build_tab_online(lv_tabview_add_tab(tabview, INFO_TAB_TITLE_ONLINE));
    build_tab_calendar(lv_tabview_add_tab(tabview, INFO_TAB_TITLE_CALENDAR));
    build_tab_setting(lv_tabview_add_tab(tabview, INFO_TAB_TITLE_SETTING));
}

lv_obj_t **screen_info_get_ptr(void) { return &scr; }
