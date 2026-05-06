#ifndef CALCULATOR_APP_H
#define CALCULATOR_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl/lvgl.h"

void calculator_ui_build(lv_obj_t *screen);
lv_indev_t *app_get_keyboard_indev(void);
void app_request_quit(void);
int app_should_quit(void);

#ifdef __cplusplus
}
#endif

#endif
