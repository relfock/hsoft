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
#include <i2c/i2c.h>
#include <stdarg.h>

#include "rtc.h"

void configure_wifi_ap(void);
void core(void *pvParameters);
void nfc_task(void *pvParameters);
bool sdk_wifi_station_set_auto_connect(uint8_t);
void configure_wifi_station(void);

extern xQueueHandle nfcq;

void user_init(void)
{
    nfcq = xQueueCreate(2, sizeof(uint8_t));
    sdk_uart_div_modify(0, UART_CLK_FREQ / 115200);
    i2c_init(5, 4);

    configure_wifi_station();

    if(!sdk_wifi_station_set_auto_connect(1))
        printf("sdk_wifi_station_set_auto_connect: FAILED\n");

    //  gpio_enable(GPIO_CONFIG, GPIO_INPUT);
    //  if(!gpio_read(GPIO_CONFIG)) {
    //      printf("Entering configuration mode...\n");
    //      configure_wifi_ap();
    //  } else {
    //      printf("Entering operational mode...\n");
    //sdk_wifi_station_set_auto_connect(0);
    xTaskCreate(nfc_task, (signed char *)"nfc_task", 2048, &nfcq, 2, NULL);
    xTaskCreate(core, (signed char *)"core", 2048, NULL, 2, NULL);
    //  }
}
