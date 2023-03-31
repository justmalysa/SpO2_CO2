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
    float co2_val;
    enum sensor_type type = SENSOR_CO2;

    while(1)
    {
        app_co2_measurement_start();
        co2_val = app_co2_val_get();
        LOG_INF("CO2 val: %f", co2_val);
        display_print(type, (uint8_t)co2_val);
        k_sleep(K_SECONDS(1));
    }
}
