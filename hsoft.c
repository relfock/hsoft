#include "espressif/esp_common.h"
#include "espressif/sdk_private.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"

int s0 = 14;
int s1 = 13;
int s2 = 12;
int rel = 5;

int led = 5;

uint16_t sdk_system_adc_read(void);

void hsoft(void *pvParameters)
{
    uint16_t adc;

    gpio_enable(s0, GPIO_OUTPUT);
    gpio_enable(s1, GPIO_OUTPUT);
    gpio_enable(s2, GPIO_OUTPUT);
    gpio_enable(rel, GPIO_OUTPUT);
    gpio_enable(led, GPIO_OUTPUT);

    gpio_write(s0, 1);
    gpio_write(s1, 0);
    gpio_write(s2, 0);

    for(;;) {
        adc = sdk_system_adc_read();
        printf("adc[%d]\n", adc);

        if(adc > 500)
            gpio_write(rel, 1);
        else 
            gpio_write(rel, 0);

        gpio_toggle(rel);
        vTaskDelay(500 / portTICK_RATE_MS);
    }
}

void user_init(void)
{
    sdk_uart_div_modify(0, UART_CLK_FREQ / 9600);
    sdk_wifi_station_set_auto_connect(0);
    xTaskCreate(hsoft, (signed char *)"hsoft", 256, NULL, 2, NULL);
}
