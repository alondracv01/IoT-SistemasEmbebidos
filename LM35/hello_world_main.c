


/* void delayMs(uint16_t ms)
{
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

static gpio_num_t dht_gpio;
static gpio_num_t lm35_gpio;
static int64_t last_read_time = -2000000;
static struct dht11_reading last_read;

#define SPP_TAG "LECTURA DEL SENSOR DHT11"

static int _waitOrTimeout(uint16_t microSeconds, int level) {
    int micros_ticks = 0;
    while(gpio_get_level(dht_gpio) == level) { 
        if(micros_ticks++ > microSeconds) 
            return DHT11_TIMEOUT_ERROR;
        ets_delay_us(1);
    }
    ESP_LOGE(SPP_TAG, "Wait or Timeout : %d microticks", micros_ticks);
    return micros_ticks;
}

static int _checkCRC(uint8_t data[]) {
    if(data[4] == (data[0] + data[1] + data[2] + data[3]))
        return DHT11_OK;
    else
        return DHT11_CRC_ERROR;
}

static void _sendStartSignal() {
    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT);
    gpio_set_level(dht_gpio, 0);
    ets_delay_us(20 * 1000);
    gpio_set_level(dht_gpio, 1);
    ets_delay_us(40);
    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT);
    ESP_LOGE(SPP_TAG, "GPIO configurado");
}

static int _checkResponse() {
    // Wait for next step ~80us
    if(_waitOrTimeout(80, 0) == DHT11_TIMEOUT_ERROR)
        return DHT11_TIMEOUT_ERROR;

    // Wait for next step ~80us
    if(_waitOrTimeout(80, 1) == DHT11_TIMEOUT_ERROR) 
        return DHT11_TIMEOUT_ERROR;

    return DHT11_OK;
}

static struct dht11_reading _timeoutError() {
    struct dht11_reading timeoutError = {DHT11_TIMEOUT_ERROR, -1, -1};
    return timeoutError;
}

static struct dht11_reading _crcError() {
    struct dht11_reading crcError = {DHT11_CRC_ERROR, -1, -1};
    return crcError;
}

void DHT11_init(gpio_num_t gpio_num) {
    // Wait 1 seconds to make the device pass its initial unstable status 
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    dht_gpio = gpio_num;
    ESP_LOGE(SPP_TAG, "Sensor configurado");
}

struct dht11_reading DHT11_read() {
    // Tried to sense too son since last read (dht11 needs ~2 seconds to make a new read) 
    if(esp_timer_get_time() - 2000000 < last_read_time) {
        ESP_LOGE(SPP_TAG, "Sensor last reading");
        return last_read;
    } 

    last_read_time = esp_timer_get_time();

    uint8_t data[5] = {0,0,0,0,0};

    _sendStartSignal();

    if(_checkResponse() == DHT11_TIMEOUT_ERROR)
        return last_read = _timeoutError();
    
    // Read response 
    for(int i = 0; i < 40; i++) {
        // Initial data 
        if(_waitOrTimeout(50, 0) == DHT11_TIMEOUT_ERROR)
            return last_read = _timeoutError();
                
        if(_waitOrTimeout(70, 1) > 28) {
            //Bit received was a 1 
            data[i/8] |= (1 << (7-(i%8)));
        }
    }

    if(_checkCRC(data) != DHT11_CRC_ERROR) {
        last_read.status = DHT11_OK;
        last_read.temperature = data[2];
        last_read.humidity = data[0];
        return last_read;
    } else {
        ESP_LOGE(SPP_TAG, "Sensor else");
        return last_read = _crcError();
    }
}

void app_main(void)
{
    printf("Hello world!\n");

   DHT11_init(GPIO_NUM_4);

    while(1) {
        printf("Temperature is %d \n", DHT11_read().temperature);
        printf("Humidity is %d\n", DHT11_read().humidity);
        printf("Status code is %d\n", DHT11_read().status);
        delayMs(1000);
    } 
}*/

#include <stdio.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "rom/ets_sys.h"
#include <driver/adc.h>
#include "esp_now.h"
#include "esp_wifi.h"

#include "freertos/event_groups.h"
#include "esp_event.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define TAG "LECTURA DEL SENSOR LM35"

#define serverNameTemp "http://192.168.4.1/temperatura"


#define ADC_VREF_mV    3300.0 // in millivolt
#define ADC_RESOLUTION 4096.0

static gpio_num_t lm35_gpio;

void delayMs(uint16_t ms){
    vTaskDelay(ms / portTICK_PERIOD_MS);
}

void LM35_init(gpio_num_t gpio_num) {
    // Wait 1 seconds to make the device pass its initial unstable status 
    delayMs(1000);
    lm35_gpio = gpio_num;
    gpio_set_direction(lm35_gpio, GPIO_MODE_INPUT);
    ESP_LOGE(TAG, "GPIO configurado");
    ESP_LOGE(TAG, "Sensor LM35 configurado");
}

int read_analog_voltage(){
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC1_CHANNEL_0,ADC_ATTEN_DB_0);
    delayMs(100);
    int val = adc1_get_raw(ADC1_CHANNEL_0);
    return val;
}

float return_temperature_celsius(){
    int analog_value = read_analog_voltage();
    float milliVolt = analog_value * (ADC_VREF_mV / ADC_RESOLUTION);
    float tempC = milliVolt / 10;
    return tempC;
}

float return_temperature_farenheit(){
    int analog_value = read_analog_voltage();
    float milliVolt = analog_value * (ADC_VREF_mV / ADC_RESOLUTION);
    float tempF = return_temperature_celsius() * 9 / 5 + 32;
    return tempF;
}

void app_main(void)
{
    printf("Hello world!\n");

   LM35_init(GPIO_NUM_4);

    int analog_value = read_analog_voltage();
    printf("Temperature is %d \n", analog_value);
    float milliVolt = analog_value * (ADC_VREF_mV / ADC_RESOLUTION);
    float tempC = milliVolt / 10;
    printf("Temperature is %fC \n", tempC);
    float tempF = tempC * 9 / 5 + 32;
    printf("Temperature is %f F \n", tempF);
    delayMs(1000);

}
