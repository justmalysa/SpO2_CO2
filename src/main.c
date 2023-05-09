#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#include "display.h"
#include "button.h"
#include "spo2.h"
#include "co2.h"

void main(void)
{
    LOG_INF("App start");

    button_cb_t buttons_cb[BUTTON_TOP] = {spo2_button_pressed, co2_button_pressed};
    display_init();
    button_init(buttons_cb);
    spo2_init();
    co2_init();

    while(1)
    {
        k_sleep(K_SECONDS(1));
    }
}
