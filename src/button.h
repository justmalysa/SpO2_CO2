#ifndef BUTTON_H
#define BUTTON_H

#include <zephyr/drivers/gpio.h>

typedef void (*button_cb_t) (void);

enum button_type
{
    BUTTON_SPO2,
    BUTTON_CO2,

    BUTTON_TOP,
};

void button_init(button_cb_t *user_button_cb);

#endif /* BUTTON_H */