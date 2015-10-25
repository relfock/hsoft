#include "espressif/esp_common.h"
#include "espressif/sdk_private.h"
#include "FreeRTOS.h"
#include "task.h"
#include "esp8266.h"
#include "hsoft.h"

#include <string.h>

#include <espressif/esp_common.h>
#include <espressif/sdk_private.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include <lwip/api.h>

void core(void *pvParameters);
bool sdk_wifi_station_set_auto_connect(uint8_t);

void configure_wifi_ap(void);
void configure_wifi_station(void);

void user_init(void)
{
    sdk_uart_div_modify(0, UART_CLK_FREQ / 115200);

    gpio_enable(GPIO_CONFIG, GPIO_INPUT);
    if(!gpio_read(GPIO_CONFIG)) {
        printf("Entering configuration mode...\n");
        configure_wifi_ap();
    } else {
        printf("Entering operational mode...\n");
        configure_wifi_station();
        xTaskCreate(core, (signed char *)"core", 256, NULL, 2, NULL);
    }
}
