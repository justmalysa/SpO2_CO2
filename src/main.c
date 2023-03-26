#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#include "app.h"
#include "display.h"

void main(void)
{
    LOG_INF("App start");

    app_init();

    uint16_t spo2_val;
    enum sensor_type type = SENSOR_SPO2;

    while(1)
    {
        app_spo2_measurement_start();
        spo2_val = app_spo2_val_get();
        LOG_INF("SpO2 val: %d", spo2_val);
        display_print(type, spo2_val);
        k_sleep(K_SECONDS(6));
    }
}
