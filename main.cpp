#include <Arduino.h>

#include <lvgl.h>

#include "app_config.h"
#include "display.h"
#include "sd_card.h"
#include "ui.h"

void setup()
{
  delay(APP_BOOT_DELAY_MS);
  Serial.begin(APP_SERIAL_BAUD);
  display_init();
  sd_card_init();
  ui_init();
}

void loop()
{
  lv_timer_handler();
  display_power_tick();
  vTaskDelay(APP_MAIN_LOOP_DELAY_MS);
}
