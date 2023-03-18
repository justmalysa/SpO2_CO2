#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

#include "math.h"

#define SPO2_MEASUREMENT_PERIOD_S    15
#define SPO2_SAMPLING_TIME_MS        10

struct spo2_ctx
{
    uint64_t ir_sum;
    uint64_t red_sum;
    uint16_t index;
    uint16_t current_val;
    struct k_timer measurement_timer;
    struct k_timer sampling_timer;
    struct k_work measurement_work;
    struct k_work sampling_work;
};

static struct spo2_ctx spo2;

/*
 * Get a device structure from a devicetree node with compatible "maxim,max30102".
 */
static const struct device *get_max30102_device(void)
{
    const struct device *dev = DEVICE_DT_GET_ANY(maxim_max30102);

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

static uint16_t spo2_calculate(void)
{
    float AC_red = sqrt((float)spo2.red_sum / SPO2_MEASUREMENT_PERIOD_S);
    float DC_red = (float)spo2.red_sum / spo2.index;
    float AC_ir = sqrt((float)spo2.ir_sum / SPO2_MEASUREMENT_PERIOD_S);
    float DC_ir = (float)spo2.ir_sum / spo2.index;
    return 110.0 - 25.0 * ((AC_red / DC_red) / (AC_ir / DC_ir));
}

static void spo2_sample_add_workqueue(struct k_work *item)
{
    const struct device *dev = get_max30102_device();

    if (dev == NULL)
    {
        return;
    }

    struct sensor_value data[2];

    if (sensor_sample_fetch(dev) < 0)
    {
        LOG_ERR("Error when fetching the data\n");
    }

    if (sensor_channel_get(dev, SENSOR_CHAN_RED, &data[0]) < 0)
    {
        LOG_ERR("Channel RED get error\n");
        return;
    }

    if (sensor_channel_get(dev, SENSOR_CHAN_IR, &data[1]) < 0)
    {
        LOG_ERR("Channel IR get error\n");
        return;
    }

    spo2.red_sum += data[0].val1;
    spo2.ir_sum += data[1].val1;
    spo2.index++;
}

static void spo2_measurement_complete_workqueue(struct k_work *item)
{
    k_timer_stop(&spo2.sampling_timer);
    spo2.current_val = spo2_calculate();
    spo2.ir_sum = 0;
    spo2.red_sum = 0;
    spo2.index = 0;
}

static void spo2_sampling_timer_expiry(struct k_timer *timer_id)
{
    k_work_submit(&spo2.sampling_work);
}

static void spo2_measurement_timer_expiry(struct k_timer *timer_id)
{
    k_work_submit(&spo2.measurement_work);
}

void app_init(void)
{
    k_timer_init(&spo2.sampling_timer, spo2_sampling_timer_expiry, NULL);
    k_timer_init(&spo2.measurement_timer, spo2_measurement_timer_expiry, NULL);
    k_work_init(&spo2.sampling_work, spo2_sample_add_workqueue);
    k_work_init(&spo2.measurement_work, spo2_measurement_complete_workqueue);
}

void app_spo2_measurement_start(void)
{
    k_timer_start(&spo2.sampling_timer, K_NO_WAIT, K_MSEC(SPO2_SAMPLING_TIME_MS));
    k_timer_start(&spo2.measurement_timer, K_SECONDS(SPO2_MEASUREMENT_PERIOD_S), K_FOREVER);
}

uint16_t app_spo2_val_get(void)
{
    return spo2.current_val;
}