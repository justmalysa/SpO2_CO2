#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/gpio.h>
#include <lvgl.h>
#include <stdio.h>
#include <string.h>
#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(display, CONFIG_LOG_DEFAULT_LEVEL);

#include "display.h"

#define SENSOR_VAL_OFFSET_X    70
#define SPO2_TEXT_OFFSET_X     16
#define CO2_TEXT_OFFSET_X      25
#define SPO2_OFFSET_Y          14
#define CO2_OFFSET_Y           40


struct display_ctx
{
    const struct device *device;
    lv_obj_t *val_label;
};

static struct display_ctx display;

void display_init(void)
{
    lv_obj_t *spo2_label;
    lv_obj_t *co2_label;

    display.device = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    if (!device_is_ready(display.device))
    {
        LOG_ERR("Device is not ready");
        return;
    }

    spo2_label = lv_label_create(lv_scr_act());
    co2_label = lv_label_create(lv_scr_act());

    lv_label_set_text(spo2_label, "SpO2 :");
    lv_obj_align(spo2_label, LV_ALIGN_TOP_LEFT, SPO2_TEXT_OFFSET_X, SPO2_OFFSET_Y);


    lv_label_set_text(co2_label, "CO2 :");
    lv_obj_align(co2_label, LV_ALIGN_TOP_LEFT, CO2_TEXT_OFFSET_X, CO2_OFFSET_Y);

    lv_task_handler();
    display_blanking_off(display.device);

    display.val_label = lv_label_create(lv_scr_act());
}

void display_print(enum sensor_type type, uint8_t val)
{
    switch (type)
    {
        case SENSOR_SPO2:
            lv_label_set_text_fmt(display.val_label, "%d %%", val);
            lv_obj_align(display.val_label, LV_ALIGN_TOP_LEFT, SENSOR_VAL_OFFSET_X, SPO2_OFFSET_Y);
            break;
        case SENSOR_CO2:
            lv_label_set_text_fmt(display.val_label, "%d %%", val);
            lv_obj_align(display.val_label, LV_ALIGN_TOP_LEFT, SENSOR_VAL_OFFSET_X, CO2_OFFSET_Y);
            break;

        default:
            break;
    }

    lv_task_handler();
    display_blanking_off(display.device);
}