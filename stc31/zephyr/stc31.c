/*
 * SPDX-License-Identifier: Apache-2.0
 */

#define DT_DRV_COMPAT sensirion_stc31

#include "zephyr/logging/log.h"

#include "stc31.h"

LOG_MODULE_REGISTER(STC31, CONFIG_SENSOR_LOG_LEVEL);

static uint8_t compute_crc(uint8_t data[], uint8_t len)
{
    uint8_t crc = 0xFF;

    for (uint8_t i = 0; i < len; i++)
    {
        crc ^= data[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            if ((crc & 0x80) != 0)
            {
                crc = (uint8_t)((crc << 1) ^ 0x31);
            }
            else
            {
                crc <<= 1;
            }
        }
    }

    return crc;
}

static int stc31_sample_fetch(const struct device *dev, enum sensor_channel chan)
{
    struct stc31_data *data = dev->data;
    const struct stc31_config *config = dev->config;

    uint8_t write_buffer[2] = {STC31_CMD_MEASURE_GAS_CONCENTRATION >> 8,
                               (uint8_t)STC31_CMD_MEASURE_GAS_CONCENTRATION};

    if (i2c_write_dt(&config->i2c, write_buffer, sizeof(write_buffer)))
    {
        LOG_ERR("Could not start measuring");
    }

    k_sleep(K_MSEC(75));

    uint8_t read_buffer[3] = {0};
    uint8_t crc = 0;

    if (i2c_read_dt(&config->i2c, read_buffer, sizeof(read_buffer)))
    {
        LOG_ERR("Could not fetch sample");
        return -EIO;
    }

    crc = compute_crc(&read_buffer[0], 2);

    if (crc == read_buffer[2])
    {
        data->raw = ((uint16_t)read_buffer[0] << 8) | read_buffer[1];
    }
    else
    {
        LOG_ERR("Measured and computed CRCs do not match");
        return -EIO;
    }

    return 0;
}

static int stc31_channel_get(const struct device *dev, enum sensor_channel chan, struct sensor_value *val)
{
    struct stc31_data *data = dev->data;

    val->val1 = data->raw;
    val->val2 = 0;

    return 0;
}

static const struct sensor_driver_api stc31_driver_api =
{
    .sample_fetch = stc31_sample_fetch,
    .channel_get = stc31_channel_get,
};

static int stc31_init(const struct device *dev)
{
    const struct stc31_config *config = dev->config;
    uint32_t part_id;

    if (!device_is_ready(config->i2c.bus)) {
        LOG_ERR("Bus device is not ready");
        return -ENODEV;
    }

    uint8_t write_buffer[2] = {STC31_CMD_READ_PRODUCT_IDENTIFIER_1 >> 8,
                               (uint8_t)STC31_CMD_READ_PRODUCT_IDENTIFIER_1};

    if (i2c_write_dt(&config->i2c, write_buffer, sizeof(write_buffer)))
    {
        LOG_ERR("Could not write product identifier command code");

        LOG_INF("Attempt to recover the bus");
        if (i2c_recover_bus(config->i2c.bus))
        {
            LOG_ERR("Bus recovery failed");
            return -EIO;
        }
        if (i2c_write_dt(&config->i2c, write_buffer, sizeof(write_buffer)))
        {
            LOG_ERR("Attempt to recover the bus failed");
            return -EIO;
        }
    }

    write_buffer[0] = STC31_CMD_READ_PRODUCT_IDENTIFIER_2 >> 8;
    write_buffer[1] = (uint8_t)STC31_CMD_READ_PRODUCT_IDENTIFIER_2;

    uint8_t read_buffer[5] = {0};

    /* Check the part id to make sure this is STC31 */
    if (i2c_write_read_dt(&config->i2c, write_buffer, sizeof(write_buffer), read_buffer, sizeof(read_buffer)))
    {
        LOG_ERR("Could not get Part ID");
        return -EIO;
    }

    part_id = (read_buffer[0] << 24) | (read_buffer[1] << 16) | (read_buffer[3] << 8) | read_buffer[4];

    if (part_id != STC31_PART_ID)
    {
        LOG_ERR("Got Part ID 0x%02x, expected 0x%02x", part_id, STC31_PART_ID);
        return -EIO;
    }

    /* Set binary gas */
    uint8_t buffer[5] = {STC31_CMD_SET_BINARY_GAS >> 8,
                         (uint8_t)STC31_CMD_SET_BINARY_GAS,
                         STC31_ARG_CO2_IN_AIR_100 >> 8,
                         (uint8_t)STC31_ARG_CO2_IN_AIR_100};

    buffer[4] = compute_crc(&buffer[2], 2);

    if (i2c_write_dt(&config->i2c, buffer, sizeof(buffer)))
    {
        LOG_ERR("Could not set binary gas");
        return -EIO;
    }

    /* Forced recalibration */
    uint16_t concentration = (uint16_t)(((STC31_FRC_REFERENCE_CONCENTRATION * 32768) / 100) + 16384);
    buffer[0] = STC31_CMD_FRC >> 8;
    buffer[1] = (uint8_t)STC31_CMD_FRC;
    buffer[2] = concentration >> 8;
    buffer[3] = (uint8_t)concentration;

    buffer[4] = compute_crc(&buffer[2], 2);

    if (i2c_write_dt(&config->i2c, buffer, sizeof(buffer)))
    {
        LOG_ERR("Could not force recalibration");
        return -EIO;
    }

    return 0;
}

static struct stc31_config stc31_config =
{
    .i2c = I2C_DT_SPEC_INST_GET(0),
};

static struct stc31_data stc31_data;

SENSOR_DEVICE_DT_INST_DEFINE(0, stc31_init, NULL, &stc31_data, &stc31_config,
    POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &stc31_driver_api);
