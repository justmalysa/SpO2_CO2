#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(app, CONFIG_LOG_DEFAULT_LEVEL);

#include "math.h"
#include "display.h"
#include "button.h"

#define SPO2_MEASUREMENT_PERIOD_S    5
#define SPO2_SAMPLING_TIME_MS        10
#define SPO2_BUFFER_SIZE             (SPO2_MEASUREMENT_PERIOD_S * 1000) / SPO2_SAMPLING_TIME_MS

#define CO2_MEASUREMENT_PERIOD_S     1


struct spo2_ctx
{
    uint16_t index;
    uint32_t red_buf[SPO2_BUFFER_SIZE];
    uint32_t ir_buf[SPO2_BUFFER_SIZE];
    uint8_t current_val;
    bool measurement_in_progress;
    struct k_timer measurement_timer;
    struct k_timer sampling_timer;
    struct k_work button_pressed;
    struct k_work sampling_work;
};

static struct spo2_ctx spo2;

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

static uint8_t spo2_calculate(void)
{
    uint64_t red_sum = 0;
    uint64_t ir_sum = 0;
    uint64_t red_squared_sum = 0;
    uint64_t ir_squared_sum = 0;
    uint32_t red_mean = 0;
    uint32_t ir_mean = 0;

    for (uint16_t i = 0; i < SPO2_BUFFER_SIZE; i++)
    {
        red_sum += spo2.red_buf[i];
        ir_sum += spo2.ir_buf[i];
    }

    red_mean = red_sum / SPO2_BUFFER_SIZE;
    ir_mean = ir_sum / SPO2_BUFFER_SIZE;

    for (uint16_t i = 0; i < SPO2_BUFFER_SIZE; i++)
    {
        red_squared_sum += (spo2.red_buf[i] - red_mean) * (spo2.red_buf[i] - red_mean);
        ir_squared_sum += (spo2.ir_buf[i] - ir_mean) * (spo2.ir_buf[i] - ir_mean);
    }

    double AC_red = sqrt((double)red_squared_sum / (double)SPO2_MEASUREMENT_PERIOD_S);
    double DC_red = (double)red_mean;
    double AC_ir = sqrt((double)ir_squared_sum / (double)SPO2_MEASUREMENT_PERIOD_S);
    double DC_ir = (double)ir_mean;

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

    spo2.red_buf[spo2.index] = data[0].val1;
    spo2.ir_buf[spo2.index] = data[1].val1;
    spo2.index++;
}

static void spo2_val_init(void)
{
    memset(spo2.red_buf, 0, SPO2_BUFFER_SIZE);
    memset(spo2.ir_buf, 0, SPO2_BUFFER_SIZE);
    spo2.index = 0;
    spo2.measurement_in_progress = false;
}

static void spo2_button_pressed_workqueue(struct k_work *item)
{
    k_timer_stop(&spo2.sampling_timer);
    spo2.current_val = spo2_calculate();
    spo2_val_init();
    display_print(SENSOR_SPO2, spo2.current_val);
}

static void spo2_sampling_timer_expiry(struct k_timer *timer_id)
{
    k_work_submit(&spo2.sampling_work);
}

static void spo2_measurement_timer_expiry(struct k_timer *timer_id)
{
    k_work_submit(&spo2.button_pressed);
}

static void spo2_button_pressed(void)
{
    if (spo2.measurement_in_progress)
    {
        return;
    }

    spo2.measurement_in_progress = true;
    k_timer_start(&spo2.sampling_timer, K_NO_WAIT, K_MSEC(SPO2_SAMPLING_TIME_MS));
    k_timer_start(&spo2.measurement_timer, K_SECONDS(SPO2_MEASUREMENT_PERIOD_S), K_FOREVER);
}

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

static void co2_button_pressed(void)
{
    if (co2.state != CO2_MEAS_NONE)
    {
        return;
    }

    co2.state = CO2_MEAS_REQUESTED;
}

void app_init(void)
{
    button_cb_t buttons_cb[BUTTON_TOP] = {spo2_button_pressed, co2_button_pressed};
    display_init();
    button_init(buttons_cb);

    k_timer_init(&spo2.sampling_timer, spo2_sampling_timer_expiry, NULL);
    k_timer_init(&spo2.measurement_timer, spo2_measurement_timer_expiry, NULL);
    k_work_init(&spo2.sampling_work, spo2_sample_add_workqueue);
    k_work_init(&spo2.button_pressed, spo2_button_pressed_workqueue);

    k_timer_init(&co2.measurement_timer, co2_measurement_timer_expiry, NULL);
    k_work_init(&co2.measurement_work, co2_measurement_complete_workqueue);
    k_timer_start(&co2.measurement_timer, K_SECONDS(CO2_MEASUREMENT_PERIOD_S), K_SECONDS(CO2_MEASUREMENT_PERIOD_S));
}
