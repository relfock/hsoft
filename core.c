#include <stdio.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include "esp8266.h"
#include "hsoft.h"

void select_analog_input(int device);
void sdk_os_delay_us(uint16_t us);
uint16_t sdk_system_adc_read(void);

void select_analog_input(int device)
{
    switch(device) {
        case EN_VOLT: // Voltage
            gpio_write(GPIO_MUX_S0, 0);
            gpio_write(GPIO_MUX_S1, 0);
            break;
        case EN_AMP: // Amp
            gpio_write(GPIO_MUX_S0, 1);
            gpio_write(GPIO_MUX_S1, 0);
            break;
        case EN_TEMP: // Temp
            gpio_write(GPIO_MUX_S0, 0);
            gpio_write(GPIO_MUX_S1, 1);
            break;
        default:
            printf("Wrong device %d\n", device);
            return;
    }

    sdk_os_delay_us(MUX_UDELAY);
}

void core(void *pvParameters)
{
    int pressed, watt;
    int volt, amp, temp;

    gpio_enable(GPIO_MUX_S0, GPIO_OUTPUT);
    gpio_enable(GPIO_MUX_S1, GPIO_OUTPUT);
    gpio_enable(GPIO_RELAY, GPIO_OUTPUT);
    gpio_enable(GPIO_BUTTON, GPIO_INPUT);

    for(;;) {
        select_analog_input(EN_VOLT);
        volt = sdk_system_adc_read();

        select_analog_input(EN_AMP);
        amp = sdk_system_adc_read();

        watt = volt * amp;

        select_analog_input(EN_TEMP);
        temp = sdk_system_adc_read();

        printf("Temp[%d]\n", temp);
        printf("Amp[%d]\n", amp);
        printf("watt[%d]\n", watt);
        printf("Voltage[%d]\n", volt);

        pressed = gpio_read(GPIO_BUTTON);
        printf("Button [%d]\n", pressed);

        if(pressed)
            gpio_write(GPIO_RELAY, 1);
        else 
            gpio_write(GPIO_RELAY, 0);

        printf("---------------------\n");

        vTaskDelay(2 * 1000 / portTICK_RATE_MS);
    }
}
