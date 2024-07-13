
#include "zboss_api.h"
#include "esp_zb_p1reader.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ha/esp_zigbee_ha_standard.h"
#include "settings.h"

#include "driver/gpio.h"
#include "driver/uart.h"

#include <stdint.h>
#include <stdlib.h>  // For rand
#include <time.h>    // For time

#include "utils.h"

/*------ Global definitions -----------*/
static char manufacturer[16], model[16], firmware_version[16];
bool time_updated = false, connected = false, DEMO_MODE = true; /*< DEMO_MDE disable all real sensors and send fake data*/
int lcd_timeout = 30;
uint8_t screen_number = 0; 
uint16_t temperature = 0, humidity = 0, pressure = 0, CO2_value = 0;
float temp = 0, pres = 0, hum = 0;
char strftime_buf[64];
SemaphoreHandle_t i2c_semaphore = NULL;
static const char *TAG = "ESP_P1_METER";

typedef struct {
  uint32_t high;
  uint32_t low;
} uint48_t;

typedef struct {
  uint32_t high;
  uint32_t mid;
  uint32_t low;
} uint56_t;

#if !defined CONFIG_ZB_ZCZR
#error Define ZB_ZCZR in idf.py menuconfig to compile light (Router) source code.
#endif

/********************* Define functions **************************/
static void configure_led(void)
{
    ESP_LOGI(TAG, "Example configured to blink GPIO LED!");
    gpio_reset_pin(BLINK_GPIO);
    /* Set the GPIO as a push/pull output */
    gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT);
}
/***********************************
            Setup Methods
 ***********************************/

/**
   setupDataReadout()

   This method can be used to create more data readout to mqtt topic.
   Use the name for the mqtt topic.
   The code for finding this in the telegram see
    https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf for the dutch codes pag. 19 -23
   Use startChar and endChar for setting the boundies where the value is in between.
   Default startChar and endChar is '(' and ')'
   Note: Make sure when you add or remove telegramObject to update the NUMBER_OF_READOUTS accordingly.
*/

void setupDataReadout()
{
    
    // 0-0:96.14.0(0001)
    // 0-0:96.14.0 = Actual Tarif
    strcpy(telegramObjects[0].name, "energy_tariff");
    telegramObjects[0].attributeID = P1_ATTRIBUTE_ENERGY_TARIFF;
    strcpy(telegramObjects[0].code, "0-0:96.14.0");
    telegramObjects[0].startChar = '(';
    telegramObjects[0].endChar = ')';
    telegramObjects[0].sendData = false;

    // 1-0:1.8.1(000992.992*kWh)
    // 1-0:1.8.1 = Elektra verbruik laag tarief (DSMR v5.0)
    strcpy(telegramObjects[1].name, "energy_low");
    telegramObjects[1].attributeID = P1_ATTRIBUTE_ENERGY_LOW;
    strcpy(telegramObjects[1].code, "1-0:1.8.1");
    telegramObjects[1].startChar = '(';
    telegramObjects[1].endChar = '*';
    telegramObjects[1].sendData = false;

    // 1-0:1.8.2(000560.157*kWh)
    // 1-0:1.8.2 = Elektra verbruik hoog tarief (DSMR v5.0)
    strcpy(telegramObjects[2].name, "energy_high");
    telegramObjects[2].attributeID = P1_ATTRIBUTE_ENERGY_HIGH;
    strcpy(telegramObjects[2].code, "1-0:1.8.2");
    telegramObjects[2].startChar = '(';
    telegramObjects[2].endChar = '*';
    telegramObjects[2].sendData = false;

    // 1-0:1.7.0(00.424*kW) Actueel verbruik
    // 1-0:1.7.x = Electricity consumption actual usage (DSMR v5.0)
    strcpy(telegramObjects[3].name, "power");
    telegramObjects[3].attributeID = P1_ATTRIBUTE_POWER;
    strcpy(telegramObjects[3].code, "1-0:1.7.0");
    telegramObjects[3].startChar = '(';
    telegramObjects[3].endChar = '*';
    telegramObjects[3].sendData = false;
    telegramObjects[3].sendThreshold = (long) 3;

    // 0-1:24.2.3(150531200000S)(00811.923*m3)
    // 0-1:24.2.3 = Gas (DSMR v5.0) on Dutch meters
    strcpy(telegramObjects[4].name, "gas");
    telegramObjects[4].attributeID = P1_ATTRIBUTE_GAS;
    strcpy(telegramObjects[4].code, "0-1:24.2.1");
    telegramObjects[4].startChar = '(';
    telegramObjects[4].endChar = '*';
    telegramObjects[4].sendData = false;
    
    // 0-0:96.7.21(00008)
    // Number of power failures in any phases
    strcpy(telegramObjects[5].name, "fail");
    telegramObjects[5].attributeID = P1_ATTRIBUTE_FAIL;
    strcpy(telegramObjects[5].code, "0-0:96.7.21");
    telegramObjects[5].startChar = '(';
    telegramObjects[5].endChar = ')';
    telegramObjects[5].sendData = false;

    // 0-0:96.7.9(00002)
    // Number of long power failures in any phases
    //telegramObjects[6].name = "fail_long";
    strcpy(telegramObjects[6].name, "fail_long");
    telegramObjects[6].attributeID = P1_ATTRIBUTE_FAIL_LONG;
    strcpy(telegramObjects[6].code, "0-0:96.7.9");
    telegramObjects[6].startChar = '(';
    telegramObjects[6].endChar = ')';
    telegramObjects[6].sendData = false;
/*
    // 1-0:21.7.0(00.378*kW)
    // 1-0:21.7.0 = Instantaan vermogen Elektriciteit levering L1
    telegramObjects[7].name = "power_l1";
    telegramObjects[7].attributeID = 0x4004;
    strcpy(telegramObjects[7].code, "1-0:21.7.0");
    telegramObjects[7].endChar = '*';
    telegramObjects[7].sendThreshold = (long) 3;

    // 1-0:41.7.0(00.378*kW)
    // 1-0:41.7.0 = Instantaan vermogen Elektriciteit levering L2
    telegramObjects[8].name = "power_l2";
    telegramObjects[8].attributeID = 0x4004;
    strcpy(telegramObjects[8].code, "1-0:41.7.0");
    telegramObjects[8].endChar = '*';
    telegramObjects[8].sendThreshold = (long) 3;

    // 1-0:61.7.0(00.378*kW)
    // 1-0:61.7.0 = Instantaan vermogen Elektriciteit levering L3
    telegramObjects[9].name = "power_l3";
    telegramObjects[9].attributeID = 0x4004;
    strcpy(telegramObjects[9].code, "1-0:61.7.0");
    telegramObjects[9].endChar = '*';
    telegramObjects[9].sendThreshold = (long) 3;
*/

#ifdef DEBUG_P1
    //Serial.println("Data initialized:");
    ESP_LOGI(TAG, "Data initialized:");
    for (int i = 0; i < NUMBER_OF_READOUTS; i++)
    {
        ESP_LOGI(TAG, "%s", telegramObjects[i].name);
       // Serial.println(telegramObjects[i].name);
    }
#endif
}


long getValue(char *buffer, int maxlen, char startchar, char endchar)
{
    int s = findCharInArrayRev(buffer, startchar, maxlen - 2);
    int l = findCharInArrayRev(buffer, endchar, maxlen) - s - 1;

    // Verify data - if message is corrupted, then there is high chance of missing start of end char which would lead to reboot of ESP
    if(s <= 0 || l <= 0) {
#ifdef DEBUG_P1
            //Serial.println((String) "Wrong input data. s: " + s + " l: " + l);
            ESP_LOGW(TAG, "Wrong input data. s: %d l: %d", s, l);
#endif
        return 0;
    }

    char res[16];
    memset(res, 0, sizeof(res));

    if (strncpy(res, buffer + s + 1, l))
    {
        if (endchar == '*')
        {
            if (isNumber(res, l))
                return (1000 * atof(res));
        }
        else if (endchar == ')')
        {
            if (isNumber(res, l))
                return atof(res);
        }
    }

    return 0;
}

/**
 *  Decodes the telegram PER line. Not the complete message. 
 */
bool decodeTelegram(int len)
{
    int startChar = findCharInArrayRev(telegram, '/', len);
    int endChar = findCharInArrayRev(telegram, '!', len);
    bool validCRCFound = false;

#ifdef DEBUG_P1
    for (int cnt = 0; cnt < len; cnt++)
    {
        printf("%c", telegram[cnt]);
        //ESP_LOGI(TAG, "%c", telegram[cnt]);
        //Serial.print(telegram[cnt]);
        
    }
    ESP_LOGI(TAG, "\n");
    //Serial.print("\n");
#endif

    if (startChar >= 0)
    {
        // * Start found. Reset CRC calculation
        currentCRC = crc16(0x0000, (unsigned char *)telegram + startChar, len - startChar);
    }
    else if (endChar >= 0)
    {
        // * Add to crc calc
        currentCRC = crc16(currentCRC, (unsigned char *)telegram + endChar, 1);

        char messageCRC[5];
        strncpy(messageCRC, telegram + endChar + 1, 4);

        messageCRC[4] = 0; // * Thanks to HarmOtten (issue 5)
        validCRCFound = (strtol(messageCRC, NULL, 16) == currentCRC);

#ifdef DEBUG_P1
        if (validCRCFound)
            ESP_LOGI(TAG, "CRC Valid!");
            //Serial.println(F("CRC Valid!"));
        else
            ESP_LOGW(TAG, "CRC Invalid! %d message %ld", currentCRC, strtol(messageCRC, NULL, 16));
            //Serial.println(F("CRC Invalid!"));
#endif
        currentCRC = 0;
    }
    else
    {
        currentCRC = crc16(currentCRC, (unsigned char *)telegram, len);
    }

    // Loops throug all the telegramObjects to find the code in the telegram line
    // If it finds the code the value will be stored in the object so it can later be send to the mqtt broker
    for (int i = 0; i < NUMBER_OF_READOUTS; i++)
    {
        if (strncmp(telegram, telegramObjects[i].code, strlen(telegramObjects[i].code)) == 0)
        {
            // Handle empty objects
            if(strlen(telegramObjects[i].code ) < 3) 
            { 
                continue; 
            };
#ifdef DEBUG_P1
            ESP_LOGI(TAG, "Got value: %s", telegram);
            //Serial.println("Get value " + String(telegram));
#endif
            long newValue = getValue(telegram, len, telegramObjects[i].startChar, telegramObjects[i].endChar);
            if (newValue != telegramObjects[i].value)
            {
                // Do not send values if they change by some really minor value
                if(abs(telegramObjects[i].value - newValue) > telegramObjects[i].sendThreshold) {

                    telegramObjects[i].sendData = true;
                }else{  
#ifdef DEBUG_P1
                    ESP_LOGI(TAG, "Value of: %s with value: %ld old %ld was rejected due to threshold", telegramObjects[i].name, newValue, telegramObjects[i].value);
                    //Serial.println((String) "Value of: " + telegramObjects[i].name + " with value: " + newValue + " old " + telegramObjects[i].value + " was rejected due to threshold");
#endif
                }
                telegramObjects[i].value = newValue;
            }
#ifdef DEBUG_P1
            ESP_LOGI(TAG, "Found a Telegram object: %s with value: %ld", telegramObjects[i].name, telegramObjects[i].value);
            //Serial.println((String) "Found a Telegram object: " + telegramObjects[i].name + " value: " + telegramObjects[i].value);
#endif
            break;
        }
    }

    return validCRCFound;
}

/* --------- User task section -----------------*/
static void get_rtc_time()
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%a %H:%M:%S", &timeinfo);    
}

static void demo_task()
{
    while (1)
    {
        if (connected)
            {  
                temperature = rand() % (3000 + 1 - 1000);
                humidity = rand() % (9000 + 1 - 2000);
                pressure = rand() % (1000 + 1 - 800);
                CO2_value = rand() % (3000 + 1 - 400);
                ESP_LOGI("DEMO MODE", "Temp = %d, Humm = %d, Press = %d, CO2_Value = %d", temperature, humidity, pressure, CO2_value);
                if (time_updated)
                {
                    get_rtc_time();
                    ESP_LOGI("DEMO MODE", "The current date/time is: %s", strftime_buf);
                }
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
    }  
}
/*----------------------------------------*/

static void bdb_start_top_level_commissioning_cb(uint8_t mode_mask)
{
    ESP_ERROR_CHECK(esp_zb_bdb_start_top_level_commissioning(mode_mask));
}
/* Manual reporting atribute to coordinator */
static void reportAttribute(uint8_t endpoint, uint16_t clusterID, uint16_t attributeID, void *value, uint8_t value_length)
{
    esp_zb_zcl_report_attr_cmd_t cmd = {
        .zcl_basic_cmd = {
            .dst_addr_u.addr_short = 0x0000,
            .dst_endpoint = endpoint,
            .src_endpoint = endpoint,
        },
        .address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT,
        .clusterID = clusterID,
        .attributeID = attributeID,
        .cluster_role = ESP_ZB_ZCL_CLUSTER_SERVER_ROLE,
    };
    // mem copy is no longer necessary after esp_zb_zcl_set_attribute_val last attribute was set to false
    //esp_zb_zcl_attr_t *value_r = esp_zb_zcl_get_attribute(endpoint, clusterID, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attributeID);
    //memcpy(value_r->data_p, value, value_length);
    esp_zb_zcl_report_attr_cmd_req(&cmd);
}

// Send single value
void sendMetric(uint32_t data, uint16_t attribute)
{
    esp_zb_lock_acquire(portMAX_DELAY);
    //esp_zb_zcl_status_t state_tmp = esp_zb_zcl_set_manufacturer_attribute_val(SENSOR_ENDPOINT, ENERGY_CUSTOM_CLUSTER, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, 0xFFFF, attribute, &data, true);
    esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ENERGY_CUSTOM_CLUSTER, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attribute, &data, false);
    esp_zb_lock_release();

    esp_zb_lock_acquire(portMAX_DELAY);
    reportAttribute(SENSOR_ENDPOINT, ENERGY_CUSTOM_CLUSTER, attribute, (uint32_t *)&data, sizeof(data));
    esp_zb_lock_release();
}

// Send 3 values at once
void sendMetric3(uint56_t data, uint16_t attribute)
{
    esp_zb_lock_acquire(portMAX_DELAY);
    esp_zb_zcl_set_attribute_val(SENSOR_ENDPOINT, ENERGY_CUSTOM_CLUSTER, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE, attribute, &data, false);
    esp_zb_lock_release();

    esp_zb_lock_acquire(portMAX_DELAY);
    reportAttribute(SENSOR_ENDPOINT, ENERGY_CUSTOM_CLUSTER, attribute, &data, sizeof(data));
    esp_zb_lock_release();
}


void sendDataToBroker()
{
    for (int i = 0; i < NUMBER_OF_READOUTS; i++)
    {
        if (telegramObjects[i].sendData)
        {
#ifdef DEBUG_P1
        ESP_LOGI(TAG,"Sending: %s value: %ld", telegramObjects[i].name, telegramObjects[i].value);
#endif
            if(telegramObjects[i].attributeID == P1_ATTRIBUTE_POWER_3L) {
                /*uint56_t report;
                telegramObjects[7].sendData = false;
                telegramObjects[8].sendData = false;
                telegramObjects[9].sendData = false;
                report.high = telegramObjects[7].value;
                report.mid = telegramObjects[8].value;
                report.low = telegramObjects[9].value;
                sendMetric3(report, telegramObjects[i].attributeID);*/

            }else{
                sendMetric(telegramObjects[i].value, telegramObjects[i].attributeID);
                telegramObjects[i].sendData = false;
            }
        }
    }
}

static void esp_delayed_off(void *pvParameters)
{
    gpio_set_level(BLINK_GPIO, 0);
    vTaskDelay((500) / portTICK_PERIOD_MS);
    gpio_set_level(BLINK_GPIO, 1);

    vTaskDelete(NULL);
}

bool readP1Serial()
{
    size_t available_bytes = 0;
    uart_get_buffered_data_len(UART_NUM, &available_bytes);

    if (available_bytes > 0) {
#ifdef DEBUG_P1
        ESP_LOGI(TAG, "Serial is available");
#endif
        //memset(telegram_full, 0, sizeof(telegram_full));
        uint8_t* telegram_full = (uint8_t*) malloc(P1_MAXLINELENGTH);
        memset(telegram_full, 0, P1_MAXLINELENGTH);
        int len = uart_read_bytes(UART_NUM, telegram_full, P1_MAXLINELENGTH, 20 / 1000);
        if (len > 0) {
            //xTaskCreate(esp_delayed_off, "Delayed_Off", 4096, NULL, 3, NULL);
            
            // Initialize a copy of the telegram to tokenize
            char *telegram_copy = strdup((char *)telegram_full);
            if (telegram_copy == NULL) {
#ifdef DEBUG_P1
                printf("Failed to allocate memory.\n");
#endif
                return false;
            }
            
            // Tokenize the telegram by line
            char *line = strtok(telegram_copy, "\n");
            while (line != NULL) 
            {
#ifdef DEBUG_P1
                ESP_LOGI(TAG, "Line: %s\n", line);
#endif                
                // Process each line here as needed
                memset(telegram, 0, sizeof(telegram));
                //strcpy(line, telegram);
                int line_len = strlen(line);
                strncpy(telegram, line, line_len);
                //int line_len = strlen(line) + 1;

                telegram[line_len] = '\r\n';
                telegram[line_len + 1] = '\0';
                bool result = decodeTelegram(line_len + 1);

#ifdef DEBUG_P1
                ESP_LOGI(TAG, "Length %d telegram: %s result: %d", line_len, telegram, strlen(telegram));
#endif
                // Get the next line
                line = strtok(NULL, "\n");

                if(result)
                    return true;
            }

            // Free the allocated memory
            free(telegram_copy);
        }
        free(telegram_full);
    }
    return false;
}

void mainTask()
{
    int64_t now = 0;
    while (1)
    {
        if (connected)
        {
            now = esp_timer_get_time() / 1000;
            // Check if we want a full update of all the data including the unchanged data.
            if (now - LAST_FULL_UPDATE_SENT > UPDATE_FULL_INTERVAL)
            {
                for (int i = 0; i < NUMBER_OF_READOUTS; i++)
                {
                    telegramObjects[i].sendData = true;
                    LAST_FULL_UPDATE_SENT = esp_timer_get_time() / 1000;
                }
            }
            /*if (now > 3600000)
            {
                // intentionally crashing after 60 minutes
                esp_zb_lock_acquire(portMAX_DELAY);
                reportAttribute(SENSOR_ENDPOINT, ENERGY_CUSTOM_CLUSTER, 343, &now, sizeof(now));
                esp_zb_lock_release();
            }*/
            if (now - LAST_UPDATE_SENT > updateInterval)
            {
                //ESP_LOGI(TAG, "Last update %ld now: %ld, update interval: %ld", now - LAST_UPDATE_SENT, now, updateInterval);
                if (readP1Serial())
                {
                    LAST_UPDATE_SENT = esp_timer_get_time() / 1000;
                    sendDataToBroker();
                    xTaskCreate(esp_delayed_off, "Delayed_Off", 4096, NULL, 3, NULL);
                    vTaskDelay(500 / portTICK_PERIOD_MS);
                }
            }else{
                uart_flush(UART_NUM);
                uart_flush_input(UART_NUM);
            }
        }else{
            gpio_set_level(BLINK_GPIO, 0);
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);
    }
}

static esp_err_t zb_attribute_handler(const esp_zb_zcl_set_attr_value_message_t *message)
{
    esp_err_t ret = ESP_OK;
    ESP_RETURN_ON_FALSE(message, ESP_FAIL, TAG, "Empty message");
    ESP_RETURN_ON_FALSE(message->info.status == ESP_ZB_ZCL_STATUS_SUCCESS, ESP_ERR_INVALID_ARG, TAG, "Received message: error status(%d)",
                        message->info.status);
    ESP_LOGI(TAG, "Received message: endpoint(%d), cluster(0x%x), attribute(0x%x), data size(%d)", message->info.dst_endpoint, message->info.cluster,
             message->attribute.id, message->attribute.data.size);
    if (message->info.dst_endpoint == SENSOR_ENDPOINT) {
        switch (message->info.cluster) {
        case ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY:
            ESP_LOGI(TAG, "Identify pressed");
            break;
            
        case ESP_ZB_ZCL_CLUSTER_ID_LEVEL_CONTROL:
            if (message->attribute.id == ESP_ZB_ZCL_ATTR_LEVEL_CONTROL_CURRENT_LEVEL_ID && message->attribute.data.type == ESP_ZB_ZCL_ATTR_TYPE_U8) {
                updateInterval = (message->attribute.data.value ? *(uint8_t *)message->attribute.data.value : updateInterval) * 1000;
                //light_driver_set_level((uint8_t)light_level);
                ESP_LOGI(TAG, "Update interval changed %lld", updateInterval);
            } else {
                ESP_LOGW(TAG, "Level Control cluster data: attribute(0x%x), type(0x%x)", message->attribute.id, message->attribute.data.type);
            }
            break;
        default:
            ESP_LOGI(TAG, "Message data: cluster(0x%x), attribute(0x%x)  ", message->info.cluster, message->attribute.id);
        }
    }
    return ret;
}

static esp_err_t zb_action_handler(esp_zb_core_action_callback_id_t callback_id, const void *message)
{
    esp_err_t ret = ESP_OK;
    switch (callback_id) {
    case ESP_ZB_CORE_SET_ATTR_VALUE_CB_ID:
        ret = zb_attribute_handler((esp_zb_zcl_set_attr_value_message_t *)message);
        break;
    default:
        ESP_LOGW(TAG, "Receive Zigbee action(0x%x) callback", callback_id);
        break;
    }
    return ret;
}

void read_server_time()
{
    esp_zb_zcl_read_attr_cmd_t read_req;
    read_req.address_mode = ESP_ZB_APS_ADDR_MODE_16_ENDP_PRESENT;
    read_req.clusterID = ESP_ZB_ZCL_CLUSTER_ID_TIME;
    read_req.zcl_basic_cmd.dst_endpoint = 1;
    read_req.zcl_basic_cmd.src_endpoint = 1;
    read_req.zcl_basic_cmd.dst_addr_u.addr_short = 0x0000;
    esp_zb_zcl_read_attr_cmd_req(&read_req);
}

void esp_zb_app_signal_handler(esp_zb_app_signal_t *signal_struct)
{
    uint32_t *p_sg_p = signal_struct->p_app_signal;
    esp_err_t err_status = signal_struct->esp_err_status;
    esp_zb_app_signal_type_t sig_type = *p_sg_p;
    switch (sig_type) {
    case ESP_ZB_ZDO_SIGNAL_SKIP_STARTUP:
        ESP_LOGI(TAG, "Zigbee stack initialized");
        esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_INITIALIZATION);
        break;
    case ESP_ZB_BDB_SIGNAL_DEVICE_FIRST_START:
    case ESP_ZB_BDB_SIGNAL_DEVICE_REBOOT:
        if (err_status == ESP_OK) {
            ESP_LOGI(TAG, "Start network steering");
            esp_zb_bdb_start_top_level_commissioning(ESP_ZB_BDB_MODE_NETWORK_STEERING);
        } else {
            ESP_LOGW(TAG, "Failed to initialize Zigbee stack (status: %s)", esp_err_to_name(err_status));
        }
        break;
    case ESP_ZB_BDB_SIGNAL_STEERING:
        if (err_status == ESP_OK) {
            esp_zb_ieee_addr_t extended_pan_id;
            esp_zb_get_extended_pan_id(extended_pan_id);
            connected = true;
            ESP_LOGI(TAG, "Joined network successfully (Extended PAN ID: %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x, PAN ID: 0x%04hx, Channel:%d, Short Address: 0x%04hx)",
                     extended_pan_id[7], extended_pan_id[6], extended_pan_id[5], extended_pan_id[4],
                     extended_pan_id[3], extended_pan_id[2], extended_pan_id[1], extended_pan_id[0],
                     esp_zb_get_pan_id(), esp_zb_get_current_channel(), esp_zb_get_short_address());
        } else {
            connected = false;
            ESP_LOGI(TAG, "Network steering was not successful (status: %s)", esp_err_to_name(err_status));
            esp_zb_scheduler_alarm((esp_zb_callback_t)bdb_start_top_level_commissioning_cb, ESP_ZB_BDB_MODE_NETWORK_STEERING, 1000);
        }
        break;
    case ESP_ZB_NWK_SIGNAL_PERMIT_JOIN_STATUS:
        if (err_status == ESP_OK) {
            if (*(uint8_t *)esp_zb_app_signal_get_params(p_sg_p)) {
                ESP_LOGI(TAG, "Network(0x%04hx) is open for %d seconds", esp_zb_get_pan_id(), *(uint8_t *)esp_zb_app_signal_get_params(p_sg_p));
            } else {
                ESP_LOGW(TAG, "Network(0x%04hx) closed, devices joining not allowed.", esp_zb_get_pan_id());
            }
        }
        break;
    default:
        ESP_LOGI(TAG, "ZDO signal: %s (0x%x), status: %s", esp_zb_zdo_signal_to_string(sig_type), sig_type, esp_err_to_name(err_status));
        break;
    }
}

static void set_zcl_string(char *buffer, char *value)
{
    buffer[0] = (char) strlen(value);
    memcpy(buffer + 1, value, buffer[0]);
}

static void esp_zb_task(void *pvParameters)
{
    /* initialize Zigbee stack */
    esp_zb_cfg_t zb_nwk_cfg = ESP_ZB_ZR_CONFIG();
    esp_zb_init(&zb_nwk_cfg);

    uint8_t test_attr, test_attr2;

    test_attr = 0;
    test_attr2 = 4;
    uint16_t undefined_value;
    undefined_value = 0x8000;
   /* basic cluster create with fully customized */
    uint8_t dc_power_source;
    dc_power_source = 4;
    esp_zb_attribute_list_t *esp_zb_basic_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_BASIC);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &test_attr);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_POWER_SOURCE_ID, &test_attr2);
    esp_zb_cluster_update_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_ZCL_VERSION_ID, &test_attr2);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MODEL_IDENTIFIER_ID, MODEL_IDENTIFIER);
    esp_zb_basic_cluster_add_attr(esp_zb_basic_cluster, ESP_ZB_ZCL_ATTR_BASIC_MANUFACTURER_NAME_ID, MANUFACTURER_NAME);

    /* identify cluster create with fully customized */
    esp_zb_attribute_list_t *esp_zb_identify_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_IDENTIFY);
    esp_zb_identify_cluster_add_attr(esp_zb_identify_cluster, ESP_ZB_ZCL_ATTR_IDENTIFY_IDENTIFY_TIME_ID, &test_attr);

    /* Temperature cluster */
    esp_zb_attribute_list_t *esp_zb_temperature_meas_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TEMP_MEASUREMENT);
    esp_zb_temperature_meas_cluster_add_attr(esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_VALUE_ID, &undefined_value);
    esp_zb_temperature_meas_cluster_add_attr(esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MIN_VALUE_ID, &undefined_value);
    esp_zb_temperature_meas_cluster_add_attr(esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_ATTR_TEMP_MEASUREMENT_MAX_VALUE_ID, &undefined_value);

    /* Humidity cluster */
    esp_zb_attribute_list_t *esp_zb_humidity_meas_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_REL_HUMIDITY_MEASUREMENT);
    esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_VALUE_ID, &undefined_value);
    esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MIN_VALUE_ID, &undefined_value);
    esp_zb_humidity_meas_cluster_add_attr(esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_ATTR_REL_HUMIDITY_MEASUREMENT_MAX_VALUE_ID, &undefined_value);

    /* Presure cluster */
    esp_zb_attribute_list_t *esp_zb_press_meas_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_PRESSURE_MEASUREMENT);
    esp_zb_pressure_meas_cluster_add_attr(esp_zb_press_meas_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_VALUE_ID, &undefined_value);
    esp_zb_pressure_meas_cluster_add_attr(esp_zb_press_meas_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MIN_VALUE_ID, &undefined_value);
    esp_zb_pressure_meas_cluster_add_attr(esp_zb_press_meas_cluster, ESP_ZB_ZCL_ATTR_PRESSURE_MEASUREMENT_MAX_VALUE_ID, &undefined_value);

    /* Time cluster */
    esp_zb_attribute_list_t *esp_zb_server_time_cluster = esp_zb_zcl_attr_list_create(ESP_ZB_ZCL_CLUSTER_ID_TIME);
    
    /* on-off cluster create with standard cluster config*/
    esp_zb_on_off_cluster_cfg_t on_off_cfg;
    on_off_cfg.on_off = ESP_ZB_ZCL_ON_OFF_ON_OFF_DEFAULT_VALUE;
    esp_zb_attribute_list_t *esp_zb_on_off_cluster = esp_zb_on_off_cluster_create(&on_off_cfg);

    esp_zb_level_cluster_cfg_t level_cfg;
    level_cfg.current_level = 5;
    esp_zb_attribute_list_t *esp_zb_level_cluster = esp_zb_level_cluster_create(&level_cfg);

    /* Custom cluster for CO2 ( standart cluster not working), solution only for HOMEd */
    const uint16_t attr_id = 0;
    const uint8_t attr_type = ESP_ZB_ZCL_ATTR_TYPE_U32;
    const uint8_t attr_access = ESP_ZB_ZCL_ATTR_ACCESS_REPORTING;
    
    esp_zb_attribute_list_t *custom_co2_attributes_list = esp_zb_zcl_attr_list_create(ENERGY_CUSTOM_CLUSTER);
    esp_zb_custom_cluster_add_custom_attr(custom_co2_attributes_list, 0, attr_type, attr_access, &undefined_value);
    esp_zb_custom_cluster_add_custom_attr(custom_co2_attributes_list, 1, attr_type, attr_access, &undefined_value);
    esp_zb_custom_cluster_add_custom_attr(custom_co2_attributes_list, 2, attr_type, attr_access, &undefined_value);
    esp_zb_custom_cluster_add_custom_attr(custom_co2_attributes_list, 3, attr_type, attr_access, &undefined_value);
    esp_zb_custom_cluster_add_custom_attr(custom_co2_attributes_list, 4, attr_type, attr_access, &undefined_value);
    esp_zb_custom_cluster_add_custom_attr(custom_co2_attributes_list, 5, attr_type, attr_access, &undefined_value);
    esp_zb_custom_cluster_add_custom_attr(custom_co2_attributes_list, 6, attr_type, attr_access, &undefined_value);
    esp_zb_custom_cluster_add_custom_attr(custom_co2_attributes_list, 7, attr_type, attr_access, &undefined_value);

    /* Create full cluster list enabled on device */
    esp_zb_cluster_list_t *esp_zb_cluster_list = esp_zb_zcl_cluster_list_create();
    esp_zb_cluster_list_add_basic_cluster(esp_zb_cluster_list, esp_zb_basic_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_identify_cluster(esp_zb_cluster_list, esp_zb_identify_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_temperature_meas_cluster(esp_zb_cluster_list, esp_zb_temperature_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_humidity_meas_cluster(esp_zb_cluster_list, esp_zb_humidity_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_pressure_meas_cluster(esp_zb_cluster_list, esp_zb_press_meas_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_time_cluster(esp_zb_cluster_list, esp_zb_server_time_cluster, ESP_ZB_ZCL_CLUSTER_CLIENT_ROLE);
    esp_zb_cluster_list_add_custom_cluster(esp_zb_cluster_list, custom_co2_attributes_list, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);
    esp_zb_cluster_list_add_on_off_cluster(esp_zb_cluster_list, esp_zb_on_off_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE); 
    esp_zb_cluster_list_add_level_cluster(esp_zb_cluster_list, esp_zb_level_cluster, ESP_ZB_ZCL_CLUSTER_SERVER_ROLE);    

    esp_zb_ep_list_t *esp_zb_ep_list = esp_zb_ep_list_create();
    esp_zb_endpoint_config_t endpoint_config = {
        .endpoint = SENSOR_ENDPOINT,
        .app_profile_id = ESP_ZB_AF_HA_PROFILE_ID,
        .app_device_id = ESP_ZB_HA_SIMPLE_SENSOR_DEVICE_ID,
        .app_device_version = 0
    };
    esp_zb_ep_list_add_ep(esp_zb_ep_list, esp_zb_cluster_list, endpoint_config);

    /* END */
    esp_zb_device_register(esp_zb_ep_list);
    esp_zb_core_action_handler_register(zb_action_handler);
    esp_zb_set_primary_network_channel_set(ESP_ZB_PRIMARY_CHANNEL_MASK);
    ESP_ERROR_CHECK(esp_zb_start(false));
    esp_zb_main_loop_iteration();
}

void app_main(void)
{
    //zero-initialize the config structure.
    gpio_config_t io_conf = {};
    //disable interrupt
    io_conf.intr_type = GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set,e.g.GPIO18/19
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    //disable pull-down mode
    io_conf.pull_down_en = 0;
    //disable pull-up mode
    io_conf.pull_up_en = 0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);

    gpio_set_direction(3, GPIO_MODE_OUTPUT);    // RF switch power on
    gpio_set_level(3, 0);
    gpio_set_direction(14, GPIO_MODE_OUTPUT);   // select external antenna
    gpio_set_level(14, 1);

    configure_led();
    gpio_set_direction(GPIO_OUTPUT_IO_0, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_OUTPUT_IO_0, 0);
    gpio_set_level(BLINK_GPIO, 1);

    // Configure UART parameters
    const uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    // Install UART driver
    uart_driver_install(UART_NUM, P1_MAXLINELENGTH, 0, 0, NULL, 0);
    uart_param_config(UART_NUM, &uart_config);
    uart_set_pin(UART_NUM, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_set_line_inverse(UART_NUM, UART_SIGNAL_RXD_INV);

    // Setup Zigbee
    esp_zb_platform_config_t config = {
        .radio_config = ESP_ZB_DEFAULT_RADIO_CONFIG(),
        .host_config = ESP_ZB_DEFAULT_HOST_CONFIG(),
    };
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_zb_platform_config(&config));

    setupDataReadout();

    //xTaskCreate(demo_task, "demo_task", 4096, NULL, 1, NULL);
    xTaskCreate(mainTask, "Main_Task", 4096, NULL, 5, NULL);
    xTaskCreate(esp_zb_task, "Zigbee_main", 4096, NULL, 6, NULL);
}
