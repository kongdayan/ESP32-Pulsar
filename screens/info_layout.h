/*
 * info_layout.h — TabView 屏（网络 / 日历 / 设置）的视图常量。
 */
#ifndef SCREENS_INFO_LAYOUT_H
#define SCREENS_INFO_LAYOUT_H

/* TabView 本体 */
/* 沿用主题默认文字色 */
#define INFO_TEXT_COLOR_DEFAULT UI_COLOR_KEEP_DEFAULT

/* TabView 本体 */
#define INFO_TAB_SIDE          LV_DIR_LEFT
#define INFO_TAB_BAR_PX        50
#define INFO_TABVIEW_W         270
#define INFO_TABVIEW_H         220

/* 三个 Tab 的标题 */
#define INFO_TAB_TITLE_ONLINE  "Online"
#define INFO_TAB_TITLE_CALENDAR "Calendar"
#define INFO_TAB_TITLE_SETTING "Setting"

/* Online：加载指示器 */
#define INFO_SPINNER_ANIM_MS   1000
#define INFO_SPINNER_ARC_DEG   90
#define INFO_SPINNER_PX        80

/* Calendar */
#define INFO_CAL_YEAR          2024
#define INFO_CAL_MONTH         12
#define INFO_CAL_DAY           17
#define INFO_CAL_W             200
#define INFO_CAL_H             200
#define INFO_CAL_OFFSET_X      (-8)
#define INFO_CAL_OFFSET_Y      (-8)

/* Setting：行布局 */
#define INFO_ROW_LABEL_X       (-70)
#define INFO_ROW_SWITCH_X      45
#define INFO_ROW_SWITCH_W      50
#define INFO_ROW_SWITCH_H      25
#define INFO_ROW_WIFI_Y        (-90)
#define INFO_ROW_BT_LABEL_Y    (-61)
#define INFO_ROW_BT_SWITCH_Y   (-60)
#define INFO_ROW_VOL_LABEL_X   (-63)
#define INFO_ROW_VOL_LABEL_Y   (-30)
#define INFO_ROW_SLIDER_X      3
#define INFO_ROW_SLIDER_Y      1
#define INFO_ROW_SLIDER_W      150
#define INFO_ROW_SLIDER_H      10
#define INFO_ROW_CHECK_X       (-25)
#define INFO_ROW_CHECK_Y       38

#define INFO_LABEL_WIFI        "WI-FI"
#define INFO_LABEL_BLUETOOTH   "Bluetooth"
#define INFO_LABEL_VOLUME      "Volume"
#define INFO_CHECK_AUTOSCAN    "Enable Auto Scan"

#define INFO_SLIDER_START_VALUE 0

#endif /* SCREENS_INFO_LAYOUT_H */
