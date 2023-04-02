#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/sys/util.h>
#include <inttypes.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(button, CONFIG_LOG_DEFAULT_LEVEL);

#include "button.h"

/* Get buttons configuration from the devicetree aliases. */

#define SPO2_BUTTON_NODE    DT_ALIAS(spo2button)
#if !DT_NODE_HAS_STATUS(SPO2_BUTTON_NODE, okay)
#error "Unsupported board: spo2button devicetree alias is not defined"
#endif

#define CO2_BUTTON_NODE    DT_ALIAS(co2button)
#if !DT_NODE_HAS_STATUS(CO2_BUTTON_NODE, okay)
#error "Unsupported board: co2button devicetree alias is not defined"
#endif

const struct gpio_dt_spec buttons[BUTTON_TOP] = {GPIO_DT_SPEC_GET_OR(SPO2_BUTTON_NODE, gpios, {0}),
                                                 GPIO_DT_SPEC_GET_OR(CO2_BUTTON_NODE, gpios, {0})};

static struct gpio_callback callbacks[BUTTON_TOP];

void button_init(button_cb_t spo2_button_cb, button_cb_t co2_button_cb)
{
    int ret;

    for (uint8_t i = 0; i < BUTTON_TOP; i++)
    {
        if (!device_is_ready(buttons[i].port))
        {
            LOG_ERR("Button device %s is not ready\n", buttons[i].port->name);
            return;
        }

        ret = gpio_pin_configure_dt(&buttons[i], GPIO_INPUT);
        if (ret != 0)
        {
            LOG_ERR("%d: failed to configure %s pin %d\n", ret, buttons[i].port->name, buttons[i].pin);
            return;
        }

        ret = gpio_pin_interrupt_configure_dt(&buttons[i], GPIO_INT_EDGE_TO_ACTIVE);
        if (ret != 0)
        {
            LOG_ERR("%d: failed to configure interrupt on %s pin %d\n", ret, buttons[i].port->name, buttons[i].pin);
            return;
        }

        if (i == BUTTON_SPO2)
        {
            gpio_init_callback(&callbacks[i], spo2_button_cb, BIT(buttons[i].pin));
        }
        else if (i == BUTTON_CO2)
        {
            gpio_init_callback(&callbacks[i], co2_button_cb, BIT(buttons[i].pin));
        }

        gpio_add_callback(buttons[i].port, &callbacks[i]);
        LOG_INF("Set up button at %s pin %d\n", buttons[i].port->name, buttons[i].pin);
    }
}