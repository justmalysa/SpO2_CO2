#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(co2, CONFIG_LOG_DEFAULT_LEVEL);

#include "display.h"
#include "co2.h"

#define CO2_MEASUREMENT_PERIOD_S     1

enum co2_measurement_state
{
    CO2_MEAS_NONE,
    CO2_MEAS_REQUESTED,
    CO2_MEAS_STARTED,

    CO2_MEAS_TOP,
};

struct co2_ctx
{
    enum co2_measurement_state state;
    struct k_timer measurement_timer;
    struct k_work measurement_work;
    struct k_work button_pressed;
};

static struct co2_ctx co2;

/*
 * Get a device structure from a devicetree node with compatible "sensirion,stc31".
 */
static const struct device *get_stc31_device(void)
{
    const struct device *dev = DEVICE_DT_GET_ANY(sensirion_stc31);

    if (dev == NULL)
    {
        /* No such node, or the node does not have status "okay". */
        LOG_ERR("\nError: no device found.\n");
        return NULL;
    }

    if (!device_is_ready(dev))
    {
        LOG_ERR("\nError: Device \"%s\" is not ready; "
                "check the driver initialization logs for errors.\n",
               dev->name);
        return NULL;
    }

    return dev;
}

static float co2_calculate(uint16_t raw_val)
{
    float co2 = (((float)raw_val - 16384.0) * 100) / 32768.0;
    LOG_INF("CO2 val: %f", co2);
    return (co2 > 0.0) ? co2 : 0.0;
}

static void co2_measurement_timer_expiry(struct k_timer *timer_id)
{
    k_work_submit(&co2.measurement_work);
}

static void co2_measurement_complete_workqueue(struct k_work *item)
{
    const struct device *dev = get_stc31_device();

    if (dev == NULL)
    {
        return;
    }

    struct sensor_value data;

    if (sensor_sample_fetch(dev) < 0)
    {
        LOG_ERR("Error when fetching the data\n");
    }

    if (sensor_channel_get(dev, SENSOR_CHAN_CO2, &data) < 0)
    {
        LOG_ERR("Channel get error\n");
        return;
    }

    switch (co2.state)
    {
        case CO2_MEAS_REQUESTED:
            co2.state = CO2_MEAS_STARTED;
            break;
        case CO2_MEAS_STARTED:
            display_print(SENSOR_CO2, co2_calculate(data.val1));
            co2.state = CO2_MEAS_NONE;
            break;
        case CO2_MEAS_NONE:
        default:
            break;
    }
}

void co2_button_pressed(void)
{
    if (co2.state != CO2_MEAS_NONE)
    {
        return;
    }

    co2.state = CO2_MEAS_REQUESTED;
}

void co2_init(void)
{
    k_timer_init(&co2.measurement_timer, co2_measurement_timer_expiry, NULL);
    k_work_init(&co2.measurement_work, co2_measurement_complete_workqueue);
    k_timer_start(&co2.measurement_timer, K_SECONDS(CO2_MEASUREMENT_PERIOD_S), K_SECONDS(CO2_MEASUREMENT_PERIOD_S));
}
