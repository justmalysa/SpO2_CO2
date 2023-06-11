/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT maxim_max30102

#include "zephyr/logging/log.h"

#include "max30102.h"

LOG_MODULE_REGISTER(MAX30102, CONFIG_SENSOR_LOG_LEVEL);


static int max30102_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct max30102_data *data = dev->data;
    const struct max30102_config *config = dev->config;
    uint8_t buffer[MAX30102_MAX_BYTES_PER_SAMPLE];
    uint32_t fifo_data;
    int fifo_chan;
    int num_bytes;
    int i;

    /* Read all the active channels for one sample */
    num_bytes = data->num_channels * MAX30102_BYTES_PER_CHANNEL;
    if (i2c_burst_read_dt(&config->i2c, MAX30102_REG_FIFO_DATA, buffer, num_bytes))
    {
        LOG_ERR("Could not fetch sample");
        return -EIO;
    }

    fifo_chan = 0;
    for (i = 0; i < num_bytes; i += 3)
    {
        /* Each channel is 18-bits */
        fifo_data = (buffer[i] << 16) | (buffer[i + 1] << 8) | (buffer[i + 2]);
        fifo_data &= MAX30102_FIFO_DATA_MASK;

        /* Save the raw data */
        data->raw[fifo_chan++] = fifo_data;
    }

    return 0;
}

static int max30102_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    struct max30102_data *data = dev->data;
    enum max30102_led_channel led_chan;
    int fifo_chan;

    switch (chan)
    {
    case SENSOR_CHAN_RED:
        led_chan = MAX30102_LED_CHANNEL_RED;
        break;

    case SENSOR_CHAN_IR:
        led_chan = MAX30102_LED_CHANNEL_IR;
        break;

    default:
        LOG_ERR("Unsupported sensor channel");
        return -ENOTSUP;
    }

    /* Check if the led channel is active by looking up the associated fifo
     * channel. If the fifo channel isn't valid, then the led channel
     * isn't active.
     */
    fifo_chan = data->map[led_chan];
    if (fifo_chan >= MAX30102_MAX_NUM_CHANNELS)
    {
        LOG_ERR("Inactive sensor channel");
        return -ENOTSUP;
    }

    /* TODO: Scale the raw data to standard units */
    val->val1 = data->raw[fifo_chan];
    val->val2 = 0;

    return 0;
}

static int max30102_attr_set(const struct device *dev, enum sensor_channel chan, enum sensor_attribute attr, const struct sensor_value *val)
{
    const struct max30102_config *config = dev->config;
    uint8_t led1_pa = 0x00;
    uint8_t led2_pa = 0x00;

    if ((chan != SENSOR_CHAN_RED) && (chan != SENSOR_CHAN_IR))
    {
        LOG_ERR("Not supported channel");
        return -ENOTSUP;
    }

    switch (val->val1)
    {
    case MAX30102_POWER_ON:
        led1_pa = CONFIG_MAX30102_LED1_PA;
        led2_pa = CONFIG_MAX30102_LED2_PA;
        break;

    case MAX30102_POWER_OFF:
        led1_pa = 0x00;
        led2_pa = 0x00;
        break;

    default:
        break;
    }

    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_LED1_PA, led1_pa))
    {
        return -EIO;
    }
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_LED2_PA, led2_pa))
    {
        return -EIO;
    }

    return 0;
}

static const struct sensor_driver_api max30102_driver_api =
{
    .attr_set = max30102_attr_set,
    .sample_fetch = max30102_sample_fetch,
    .channel_get = max30102_channel_get,
};

static int max30102_init(const struct device *dev)
{
    const struct max30102_config *config = dev->config;
    struct max30102_data *data = dev->data;
    uint8_t part_id;
    uint8_t mode_cfg;
    uint32_t led_chan;
    int fifo_chan;

    if (!device_is_ready(config->i2c.bus)) {
        LOG_ERR("Bus device is not ready");
        return -ENODEV;
    }

    /* Check the part id to make sure this is MAX30102 */
    if (i2c_reg_read_byte_dt(&config->i2c, MAX30102_REG_PART_ID, &part_id))
    {
        LOG_ERR("Could not get Part ID");

        LOG_INF("Attempt to recover the bus");
        if (i2c_recover_bus(config->i2c.bus))
        {
            LOG_ERR("Bus recovery failed");
            return -EIO;
        }
    }

    if (i2c_reg_read_byte_dt(&config->i2c, MAX30102_REG_PART_ID, &part_id))
    {
        LOG_ERR("Could not get Part ID");
        return -EIO;
    }

    if (part_id != MAX30102_PART_ID)
    {
        LOG_ERR("Got Part ID 0x%02x, expected 0x%02x", part_id, MAX30102_PART_ID);
        return -EIO;
    }

    /* Reset the sensor */
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_MODE_CFG, MAX30102_MODE_CFG_RESET_MASK))
    {
        return -EIO;
    }

    /* Wait for reset to be cleared */
    do
    {
        if (i2c_reg_read_byte_dt(&config->i2c, MAX30102_REG_MODE_CFG, &mode_cfg))
        {
            LOG_ERR("Could read mode cfg after reset");
            return -EIO;
        }
    } while (mode_cfg & MAX30102_MODE_CFG_RESET_MASK);

    /* Write the FIFO configuration register */
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_FIFO_CFG, config->fifo))
    {
        return -EIO;
    }

    /* Write the mode configuration register */
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_MODE_CFG, config->mode))
    {
        return -EIO;
    }

    /* Write the SpO2 configuration register */
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_SPO2_CFG, config->spo2))
    {
        return -EIO;
    }

    /* Write the LED pulse amplitude registers */
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_LED1_PA, config->led_pa[0]))
    {
        return -EIO;
    }
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_LED2_PA, config->led_pa[1]))
    {
        return -EIO;
    }
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_LED3_PA, config->led_pa[2]))
    {
        return -EIO;
    }

#ifdef CONFIG_MAX30102_MULTI_LED_MODE
    uint8_t multi_led[2];

    /* Write the multi-LED mode control registers */
    multi_led[0] = (config->slot[1] << 4) | (config->slot[0]);
    multi_led[1] = (config->slot[3] << 4) | (config->slot[2]);

    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_MULTI_LED, multi_led[0]))
    {
        return -EIO;
    }
    if (i2c_reg_write_byte_dt(&config->i2c, MAX30102_REG_MULTI_LED + 1, multi_led[1]))
    {
        return -EIO;
    }
#endif

    /* Initialize the channel map and active channel count */
    data->num_channels = 0U;
    for (led_chan = 0U; led_chan < MAX30102_MAX_NUM_CHANNELS; led_chan++)
    {
        data->map[led_chan] = MAX30102_MAX_NUM_CHANNELS;
    }

    /* Count the number of active channels and build a map that translates
     * the LED channel number (red/ir) to the fifo channel number.
     */
    for (fifo_chan = 0; fifo_chan < MAX30102_MAX_NUM_CHANNELS; fifo_chan++)
    {
        led_chan = (config->slot[fifo_chan] & MAX30102_SLOT_LED_MASK)-1;
        if (led_chan < MAX30102_MAX_NUM_CHANNELS)
        {
            data->map[led_chan] = fifo_chan;
            data->num_channels++;
        }
    }

    return 0;
}

static struct max30102_config max30102_config =
{
    .i2c = I2C_DT_SPEC_INST_GET(0),
    .fifo = (CONFIG_MAX30102_SMP_AVE << MAX30102_FIFO_CFG_SMP_AVE_SHIFT) |
#ifdef CONFIG_MAX30102_FIFO_ROLLOVER_EN
    MAX30102_FIFO_CFG_ROLLOVER_EN_MASK |
#endif
    (CONFIG_MAX30102_FIFO_A_FULL << MAX30102_FIFO_CFG_FIFO_FULL_SHIFT),

#if defined(CONFIG_MAX30102_HEART_RATE_MODE)
    .mode = MAX30102_MODE_HEART_RATE,
    .slot[0] = MAX30102_SLOT_RED_LED1_PA,
    .slot[1] = MAX30102_SLOT_DISABLED,
    .slot[2] = MAX30102_SLOT_DISABLED,
    .slot[3] = MAX30102_SLOT_DISABLED,
#elif defined(CONFIG_MAX30102_SPO2_MODE)
    .mode = MAX30102_MODE_SPO2,
    .slot[0] = MAX30102_SLOT_RED_LED1_PA,
    .slot[1] = MAX30102_SLOT_IR_LED2_PA,
    .slot[2] = MAX30102_SLOT_DISABLED,
    .slot[3] = MAX30102_SLOT_DISABLED,
#else
    .mode = MAX30102_MODE_MULTI_LED,
    .slot[0] = CONFIG_MAX30102_SLOT1,
    .slot[1] = CONFIG_MAX30102_SLOT2,
    .slot[2] = CONFIG_MAX30102_SLOT3,
    .slot[3] = CONFIG_MAX30102_SLOT4,
#endif

    .spo2 = (CONFIG_MAX30102_ADC_RGE << MAX30102_SPO2_ADC_RGE_SHIFT) |
        (CONFIG_MAX30102_SR << MAX30102_SPO2_SR_SHIFT) |
        (MAX30102_PW_18BITS << MAX30102_SPO2_PW_SHIFT),

    .led_pa[0] = 0x00,
    .led_pa[1] = 0x00,
};

static struct max30102_data max30102_data;

SENSOR_DEVICE_DT_INST_DEFINE(0, max30102_init, NULL, &max30102_data, &max30102_config,
    POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &max30102_driver_api);
