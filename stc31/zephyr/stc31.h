/*
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/drivers/sensor.h>
#include <zephyr/drivers/i2c.h>
#include <zephyr/drivers/gpio.h>

#define STC31_CMD_DISABLE_CRC                  0x3768
#define STC31_CMD_SET_BINARY_GAS               0x3615
#define STC31_CMD_SET_RELATIVE_HUMIDITY        0x3624
#define STC31_CMD_SET_TEMPERATURE              0x361E
#define STC31_CMD_SET_PRESSURE                 0x362F
#define STC31_CMD_MEASURE_GAS_CONCENTRATION    0x3639
#define STC31_CMD_FRC                          0x3661
#define STC31_CMD_ASC_EN                       0x3FEF
#define STC31_CMD_ASC_DIS                      0x3F6E
#define STC31_CMD_ASC_PREPARE_READ_STATE       0x3752
#define STC31_CMD_ASC_READ_STATE               0xE133
#define STC31_CMD_ASC_WRITE_STATE              0xE133
#define STC31_CMD_ASC_APPLY_STATE              0x3650
#define STC31_CMD_SELF_TEST                    0x365B
#define STC31_CMD_SOFT_RESET                   0x0006
#define STC31_CMD_ENTER_SLEEP_MODE             0x3677
#define STC31_CMD_READ_PRODUCT_IDENTIFIER_1    0x367C
#define STC31_CMD_READ_PRODUCT_IDENTIFIER_2    0xE102

#define STC31_ARG_CO2_IN_N2_100                0x0000
#define STC31_ARG_CO2_IN_AIR_100               0x0001
#define STC31_ARG_CO2_IN_N2_25                 0x0002
#define STC31_ARG_CO2_IN_AIR_25                0x0003

#define STC31_PART_ID    0x08010301

struct stc31_config
{
    struct i2c_dt_spec i2c;
};

struct stc31_data
{
    uint16_t raw;
};
