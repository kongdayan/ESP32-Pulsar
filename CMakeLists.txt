# 手工维护的 UI/逻辑源文件（PlatformIO 侧由 platformio.ini 的 build_src_filter 决定）
FILE(GLOB CORE_SOURCES core/*.c)
FILE(GLOB COMMON_SOURCES common/*.c)

SET(SOURCES ui_Screen1.c
    ui_Screen2.c
    ui_Screen3.c
    ui_Screen4.c
    ui.c
    ui_comp_hook.c
    ui_helpers.c
    ui_img_1539399133.c)

add_library(ui ${SOURCES} ${CORE_SOURCES} ${COMMON_SOURCES})
