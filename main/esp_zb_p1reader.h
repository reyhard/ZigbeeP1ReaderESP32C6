/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 *
 * Zigbee HA_color_dimmable_light Example
 *
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 *
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

#include "esp_zigbee_core.h"
#include "light_driver.h"

/* Zigbee configuration */
#define MAX_CHILDREN                      10                                    /* the max amount of connected devices */
#define INSTALLCODE_POLICY_ENABLE         false                                 /* enable the install code policy for security */
#define HA_DEVICE_ENDPOINT  10                                    /* esp light switch device endpoint */
#define ESP_ZB_PRIMARY_CHANNEL_MASK       ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */

#define P1_METER_CLUSTER              0xFFF2                                /* Custom cluster for handling P1 attributes*/

#define P1_ATTRIBUTE_ENERGY_LOW             0
#define P1_ATTRIBUTE_ENERGY_HIGH            1
#define P1_ATTRIBUTE_ENERGY_TARIFF          2
#define P1_ATTRIBUTE_POWER                  3
#define P1_ATTRIBUTE_POWER_3L               4
#define P1_ATTRIBUTE_GAS                    5
#define P1_ATTRIBUTE_FAIL                   6
#define P1_ATTRIBUTE_FAIL_LONG              7

#define UART_NUM UART_NUM_1
#define UART_TX_PIN  (GPIO_NUM_16)
#define UART_RX_PIN  (GPIO_NUM_17)
#define BUF_SIZE (1024)

#define GPIO_OUTPUT_IO_0    11
#define GPIO_OUTPUT_IO_1    7
// | (1ULL<<GPIO_OUTPUT_IO_1)
#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<GPIO_OUTPUT_IO_0) )

#define BLINK_GPIO 15

#define ESP_TEMP_SENSOR_UPDATE_INTERVAL (1)     /* Local sensor update interval (second) */
#define ESP_TEMP_SENSOR_MIN_VALUE       (-10)   /* Local sensor min measured value (degree Celsius) */
#define ESP_TEMP_SENSOR_MAX_VALUE       (80)    /* Local sensor max measured value (degree Celsius) */

#define SENSOR_ENDPOINT                 1
#define CO2_CUSTOM_CLUSTER              0xFFF2                                /* Custom cluster used because standart cluster not working*/
#define ESP_ZB_PRIMARY_CHANNEL_MASK     ESP_ZB_TRANSCEIVER_ALL_CHANNELS_MASK  /* Zigbee primary channel mask use in the example */
#define OTA_UPGRADE_MANUFACTURER        0x1001                                /* The attribute indicates the file version of the downloaded image on the device*/
#define OTA_UPGRADE_IMAGE_TYPE          0x1011                                /* The attribute indicates the value for the manufacturer of the device */
#define OTA_UPGRADE_FILE_VERSION        0x01010101                            /* The attribute indicates the file version of the running firmware image on the device */
#define OTA_UPGRADE_HW_VERSION          0x0101                                /* The parameter indicates the version of hardware */
#define OTA_UPGRADE_MAX_DATA_SIZE       64                                    /* The parameter indicates the maximum data size of query block image */
#define MODEL_NAME                      "Air Sensor 1.0"
#define FIRMWARE_VERSION                "ver-0.1"

#define MANUFACTURER_NAME               "\x02""Espresiff"
#define MODEL_IDENTIFIER                "\x0E""ESP32C6.P1.Meter"

#define ESP_ZB_ZR_CONFIG()                                                              \
    {                                                                                   \
        .esp_zb_role = ESP_ZB_DEVICE_TYPE_ROUTER,                                       \
        .install_code_policy = INSTALLCODE_POLICY_ENABLE,                               \
        .nwk_cfg.zczr_cfg = {                                                           \
            .max_children = MAX_CHILDREN,                                               \
        },                                                                              \
    }

#define ESP_ZB_DEFAULT_RADIO_CONFIG()                           \
    {                                                           \
        .radio_mode = ZB_RADIO_MODE_NATIVE,                        \
    }

#define ESP_ZB_DEFAULT_HOST_CONFIG()                            \
    {                                                           \
        .host_connection_mode = ZB_HOST_CONNECTION_MODE_NONE,      \
    }
