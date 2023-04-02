#include <zephyr/kernel.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(main, CONFIG_LOG_DEFAULT_LEVEL);

#include "app.h"

void main(void)
{
    LOG_INF("App start");

    app_init();

    while(1)
    {
        k_sleep(K_SECONDS(1));
    }
}
