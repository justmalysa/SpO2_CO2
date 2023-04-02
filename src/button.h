#ifndef BUTTON_H
#define BUTTON_H

#include <zephyr/drivers/gpio.h>

typedef void (*button_cb_t) (const struct device *dev, struct gpio_callback *cb, uint32_t pins);

enum button_type
{
    BUTTON_SPO2,
    BUTTON_CO2,

    BUTTON_TOP,
};

void button_init(button_cb_t spo2_button_cb, button_cb_t co2_button_cb);

#endif /* BUTTON_H */