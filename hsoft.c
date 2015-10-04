#include "espressif/esp_common.h"
#include "espressif/sdk_private.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"
#include "hsoft.h"

void sdk_os_delay_us(uint16_t us);
uint16_t sdk_system_adc_read(void);

void select_analog_input(int device)
{
    switch(device) {
        case EN_VOLT:
            // Voltage
            gpio_write(MUX_S0, 0);
            gpio_write(MUX_S1, 0);
            gpio_write(MUX_S2, 0);
            break;
        case EN_AMP:
            // Amp
            gpio_write(MUX_S0, 0);
            gpio_write(MUX_S1, 1);
            gpio_write(MUX_S2, 0);
            break;
        case EN_TEMP:
            // Temp
            gpio_write(MUX_S0, 1);
            gpio_write(MUX_S1, 0);
            gpio_write(MUX_S2, 0);
            break;
        case EN_HUMI:
            // Humidity
            gpio_write(MUX_S0, 1);
            gpio_write(MUX_S1, 1);
            gpio_write(MUX_S2, 0);
            break;
        default:
            printf("Wrong device %d\n", device);
            return;
    }

    sdk_os_delay_us(MUX_UDELAY);
}

void hsoft(void *pvParameters)
{
    int va_delay = VA_DELAY;
    int pressed, watt;
    int volt, amp, temp, humi;

    gpio_enable(MUX_S0, GPIO_OUTPUT);
    gpio_enable(MUX_S1, GPIO_OUTPUT);
    gpio_enable(MUX_S2, GPIO_OUTPUT);
    gpio_enable(RELAY, GPIO_OUTPUT);
    gpio_enable(BUTTON, GPIO_INPUT);

    for(;;) {
        select_analog_input(EN_VOLT);
        volt = sdk_system_adc_read();
        printf("Voltage[%d]\n", volt);

        select_analog_input(EN_AMP);
        amp = sdk_system_adc_read();
        printf("Amp[%d]\n", amp);

        watt = volt * amp;

        printf("watt[%d]\n", watt);

        va_delay -= TH_DELAY;

        if(va_delay <= 0) {
            va_delay = VA_DELAY;
            select_analog_input(EN_TEMP);
            temp = sdk_system_adc_read();
            printf("Temp[%d]\n", temp);

            select_analog_input(EN_HUMI);
            humi = sdk_system_adc_read();
            printf("Humidity[%d]\n", humi);
        }

        pressed = gpio_read(BUTTON);
        printf("Button [%d]\n", pressed);

        if(pressed)
            gpio_write(RELAY, 1);
        else 
            gpio_write(RELAY, 0);

        printf("---------------------\n");

        vTaskDelay(TH_DELAY * 1000 / portTICK_RATE_MS);
    }
}

enum {
    WIFI_STATION = 1,
    WIFI_SOFTAP = 2,
    WIFI_STAAP = 3
};

void wifi(void *pvParameters)
{
    int opmode;
    struct sdk_station_config config;
    
    opmode = sdk_wifi_get_opmode();
    printf("opmode %d\n", opmode);

    if(sdk_wifi_station_get_config(&config) == 0) {
        printf("Failed to get wifi station config\n");
    } else {
        printf("ssid %s password %s\n", config.ssid, config.password);
    }

    for(;;) {
        sdk_wifi_station_set_auto_connect(0);
        vTaskDelay(60000 / portTICK_RATE_MS);
    }
}

void user_init(void)
{
    sdk_uart_div_modify(0, UART_CLK_FREQ / 9600);
    xTaskCreate(hsoft, (signed char *)"hsoft", 256, NULL, 2, NULL);
    //xTaskCreate(wifi, (signed char *)"wifi", 256, NULL, 2, NULL);
}
