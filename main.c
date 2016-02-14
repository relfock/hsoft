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
#include "nfc.h"

#define BUF_SIZE 30

struct netconn *nc;
static xQueueHandle tsqueue;

void configure_wifi_ap(void);
void core(void *pvParameters);
void configure_wifi_station(void);
bool sdk_wifi_station_set_auto_connect(uint8_t);


//static void net_console(void *pvParameters)
//{
//    ip_addr_t serverip;
//    //uint8_t i2c_data = 0;
//
//    IP4_ADDR(&serverip, 192, 168, 1, 17);
//
//    vTaskDelay(5 * 1000 / portTICK_RATE_MS); nc = netconn_new(NETCONN_TCP);
//    if(!nc) {
//        printf("Status monitor: Failed to allocate socket.\r\n");
//        goto sleep;
//    }
//
//    {
//reconnect:
//        printf("connecting to 192.168.1.17:7788\n");
//        err_t err = netconn_connect(nc, &serverip, 7788);
//        if ( err != ERR_OK ) {
//            printf("Failed to connect to 192.168.1.17:7788\n");
//            vTaskDelay(2 * 1000 / portTICK_RATE_MS);
//            goto reconnect;
//        }
//
//        net_printf("Uptime %d seconds\n", xTaskGetTickCount() * portTICK_RATE_MS / 1000);
//        net_printf("Free heap %d bytes\n", (int)xPortGetFreeHeapSize());
//    }
//
//    i2c_init(5, 4);
//
//    struct rtc_time time;
//    rtc_get(&time);
//
//    if(time.tm_year != 2016)
//    {
//        // set the time if outdated
//        net_printf("setting time...\n");
//        memset(&time, 0, sizeof(struct rtc_time));
//        time.tm_year = 2016;
//        time.tm_mon = 1;
//        time.tm_mday = 17;
//        time.tm_hour = 12;
//        time.tm_min = 0;
//        time.tm_sec = 0;
//        rtc_set(&time);
//    }
//
//sleep:
//    //for(;;)
//    {
//        uint8_t addr=0; 
//        uint8_t data;
//        vTaskDelay(1 * 1000 / portTICK_RATE_MS);
//        //rtc_get(&time);
//        for(addr=0; addr < 255; addr++) {
//            vTaskDelay(1 * 10 / portTICK_RATE_MS);
//            if(i2c_slave_read(addr, 0x0, &data, 1))
//                net_printf("0x%x[OK]\n", addr);
//        }
//
//        net_printf("DONE !\n");
//        vTaskDelay(100000 * 1000 / portTICK_RATE_MS);
//        //rtc_get(&time);
//    }
//}

void nfc_tag_format(void)
{
    int i;
    uint8_t buff[30];

    crc_calc(wait_accept, sizeof(wait_accept) - 2);
    crc_calc(format_ndef, sizeof(format_ndef) - 2);

    nfc_i2c_write(&get_i2c_session, sizeof(get_i2c_session));
    nfc_i2c_write(select, sizeof(select));
    nfc_i2c_write(select_ndef, sizeof(select_ndef));

    {
        //format ndef
        nfc_i2c_write(format_ndef, sizeof(format_ndef));
        nfc_i2c_read(buff, 16); { for(i = 0; i < 16; i++) { printf("%02x ", buff[i]); } printf("\n"); }
        nfc_i2c_write(wait_accept, sizeof(wait_accept));

        vTaskDelay(1 * 100 / portTICK_RATE_MS);

        format_ndef[4] = 0x0a;
        crc_calc(format_ndef, sizeof(format_ndef) - 2);
        nfc_i2c_write(format_ndef, sizeof(format_ndef));
        nfc_i2c_read(buff, 16); { for(i = 0; i < 16; i++) { printf("%02x ", buff[i]); } printf("\n"); }
        nfc_i2c_write(wait_accept, sizeof(wait_accept));
        vTaskDelay(1 * 100 / portTICK_RATE_MS);
    }

    nfc_i2c_write(deselect, sizeof(deselect)); nfc_i2c_read(buff, 10);
}
uint8_t* nfc_tag_read(void)
{
    uint8_t buff[20];
    int nfc_data_len, block_count;
    int remnant, block_size = 0;
    uint8_t *nfc_data = NULL;
    uint8_t *ptr;

    nfc_i2c_write(&get_i2c_session, sizeof(get_i2c_session));
    nfc_i2c_write(select, sizeof(select));
    nfc_i2c_write(select_ndef, sizeof(select_ndef));

    nfc_i2c_write(read_ndef_len, sizeof(read_ndef_len));
    nfc_i2c_read(buff, 10);

    //buff[1:2] == ndef len
    nfc_data_len = (buff[1] << 8) | buff[2];

    if(nfc_data_len <= 0)
        return NULL;
    
    nfc_data = malloc(nfc_data_len);
    if(!nfc_data)
        return NULL;

    memset(nfc_data, 0, nfc_data_len + 1);

    ptr = nfc_data;

    block_count = nfc_data_len / 16;
    remnant = nfc_data_len % 16;

    read_ndef[4] = 0;
    do {
        read_ndef[4] += block_size;

        if(block_count > 0) {
            block_size = 16;
        } else {
            block_size = remnant;
            remnant = 0;
        }

        read_ndef[5] = block_size;

        crc_calc(read_ndef, sizeof(read_ndef) - 2);
        nfc_i2c_write(read_ndef, sizeof(read_ndef));
        nfc_i2c_read(buff, block_size + 5);

        //{ for(i = 0; i < block_size + 5; i++) { printf("%02x ", buff[i]); } printf("\n"); }

        memcpy(nfc_data, buff + 1, block_size);
        nfc_data += block_size;
    } while(block_count-- > 0 || remnant);

    nfc_i2c_write(deselect, sizeof(deselect));

    // read response
    nfc_i2c_read(buff, 3);

    return ptr;
}

void nfc_gpio_cfg(void)
{
    uint8_t buff[BUF_SIZE];

    crc_calc(verify, sizeof(verify) - 2);
    crc_calc(select_system, sizeof(select_system) - 2);
    crc_calc(read_gpio_cfg, sizeof(read_gpio_cfg) - 2);
    crc_calc(write_gpio_cfg, sizeof(write_gpio_cfg) - 2);

    nfc_i2c_write(&get_i2c_session, sizeof(get_i2c_session));
    nfc_i2c_write(select, sizeof(select));
    nfc_i2c_write(select_system, sizeof(select_system));
    nfc_i2c_write(verify, sizeof(verify));

    //printf("Verify response:\n");
    //// read response
    //nfc_i2c_read(buff, 5);

    //for(i = 0; i < 5; i++) {
    //    printf("%02x ", buff[i]);
    //}
    //printf("\n");
    //printf("Verify DONE\n");

    nfc_i2c_write(write_gpio_cfg, sizeof(write_gpio_cfg));
    nfc_i2c_write(read_gpio_cfg, sizeof(read_gpio_cfg));

    // read response
    nfc_i2c_read(buff, 5);

    printf("GPIO config 0x%x\n", buff[1]);

    nfc_i2c_write(deselect, sizeof(deselect));

    // read response
    nfc_i2c_read(buff, 3);
}

void gpio02_interrupt_handler(void) 
{ 
    uint32_t now = xTaskGetTickCountFromISR();
    xQueueSendToBackFromISR(tsqueue, &now, NULL);
}

void print_nfc_tag()
{
    uint8_t *ptr;
    ptr = nfc_tag_read();
    if(ptr) {
        printf("NFC DATA [%s]\n", ptr + 9);
        free(ptr);
    }

    return;
}

void nfc_task(void *pvParameters)
{
    nfc_tag_format();
    printf("Waiting for NFC interrupt on gpio 2...\n");
    xQueueHandle *tsqueue = (xQueueHandle *)pvParameters;
    gpio_set_interrupt(2, GPIO_INTTYPE_EDGE_NEG);

    //nfc_gpio_cfg();
    for(;;) {
        uint32_t irq_ts;
        xQueueReceive(*tsqueue, &irq_ts, portMAX_DELAY);
        irq_ts *= portTICK_RATE_MS;

        printf("INT RXed at %d: Waiting for RF to write data...", irq_ts);
        while(!gpio_read(2));
        printf("DONE\n");
        vTaskDelay(5 * 100 / portTICK_RATE_MS);
        print_nfc_tag();
    }
}

void user_init(void)
{
    tsqueue = xQueueCreate(2, sizeof(uint32_t));
    sdk_uart_div_modify(0, UART_CLK_FREQ / 115200);
    i2c_init(5, 4);

  //  gpio_enable(GPIO_CONFIG, GPIO_INPUT);
  //  if(!gpio_read(GPIO_CONFIG)) {
  //      printf("Entering configuration mode...\n");
  //      configure_wifi_ap();
  //  } else {
  //      printf("Entering operational mode...\n");
  //      configure_wifi_station();
    sdk_wifi_station_set_auto_connect(0);
    xTaskCreate(nfc_task, (signed char *)"gpio_intr", 2048, &tsqueue, 2, NULL);
        //xTaskCreate(tsens, (signed char *)"tsens", 2048, NULL, 2, NULL);
        //xTaskCreate(nfc_tag, (signed char *)"nfc_tag", 2048, NULL, 2, NULL);
        //xTaskCreate(net_console, (signed char *)"net_console", 2048, NULL, 2, NULL);
        //xTaskCreate(core, (signed char *)"core", 256, NULL, 2, NULL);
  //  }
}
