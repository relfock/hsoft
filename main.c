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

void core(void *pvParameters);
bool sdk_wifi_station_set_auto_connect(uint8_t);

void configure_wifi_ap(void);
void configure_wifi_station(void);
struct netconn *nc;

int net_printf(const char* format, ...)
{
    va_list ap;
    char msg[1024];

    memset(msg, 0, 1024);

    va_start(ap, format);
    vsnprintf(msg + strlen(msg), sizeof(msg)-strlen(msg), format, ap);
    va_end(ap);
    printf(msg);
    netconn_write(nc, msg, strlen(msg), NETCONN_COPY);
    return 0;
}


static void net_console(void *pvParameters)
{
    ip_addr_t serverip;
    //uint8_t i2c_data = 0;

    IP4_ADDR(&serverip, 192, 168, 1, 13);

    vTaskDelay(5 * 1000 / portTICK_RATE_MS);
    nc = netconn_new(NETCONN_TCP);
    if(!nc) {
        printf("Status monitor: Failed to allocate socket.\r\n");
        goto sleep;
    }

    {
reconnect:
        printf("connecting to 192.168.1.13:7788\n");
        err_t err = netconn_connect(nc, &serverip, 7788);
        if ( err != ERR_OK ) {
            printf("Failed to connect to 192.168.1.13:7788\n");
            vTaskDelay(2 * 1000 / portTICK_RATE_MS);
            goto reconnect;
        }

        net_printf("Uptime %d seconds\n", xTaskGetTickCount() * portTICK_RATE_MS / 1000);
        net_printf("Free heap %d bytes\n", (int)xPortGetFreeHeapSize());
    }

    i2c_init(5, 4);

    struct rtc_time time;
    rtc_get(&time);

    if(time.tm_year != 2015)
    {
        // set the time if outdated
        net_printf("setting time...\n");
        memset(&time, 0, sizeof(struct rtc_time));
        time.tm_year = 2015;
        time.tm_mon = 11;
        time.tm_mday = 16;
        time.tm_hour = 13;
        time.tm_min = 40;
        time.tm_sec = 0;
        rtc_set(&time);
    }

sleep:
    for(;;){
        vTaskDelay(1 * 1000 / portTICK_RATE_MS);
        rtc_get(&time);
    }
}

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
        xTaskCreate(net_console, (signed char *)"net_console", 2048, NULL, 2, NULL);
        //xTaskCreate(core, (signed char *)"core", 256, NULL, 2, NULL);
    }
}

