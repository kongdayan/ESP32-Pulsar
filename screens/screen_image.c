#include "ui.h"
#include "ui_screen.h"

#include "image_layout.h"

static lv_obj_t *scr = NULL;

void screen_image_init(void)
{
    scr = ui_screen_create(NAV_SCREEN_IMAGE);

    lv_obj_t *img = lv_img_create(scr);
    lv_img_set_src(img, &ui_img_1539399133);
    lv_obj_set_pos(img, IMG_OFFSET_X, IMG_OFFSET_Y);
    lv_obj_set_align(img, LV_ALIGN_CENTER);
    lv_obj_add_flag(img, LV_OBJ_FLAG_ADV_HITTEST);
    lv_obj_clear_flag(img, LV_OBJ_FLAG_SCROLLABLE);
}

lv_obj_t **screen_image_get_ptr(void) { return &scr; }
