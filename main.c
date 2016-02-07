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

uint8_t* nfc_tag_isr(void)
{
    uint8_t buff[20];
    int nfc_data_len, block_count;
    int remnant, block_size = 0;
    uint8_t *nfc_data = NULL;
    uint8_t *ptr;

    vTaskDelay(3 * 1000 / portTICK_RATE_MS);

    // select I2C
    // NDEF appl select
    // NDEF select
    // read NDEF len
    // read NDEF
    nfc_i2c_write(&get_i2c_session, sizeof(get_i2c_session));
    nfc_i2c_write(select, sizeof(select));
    nfc_i2c_write(select_ndef, sizeof(select_ndef));
    nfc_i2c_write(read_ndef_len, sizeof(read_ndef_len));

    // read response
    nfc_i2c_read(buff, 10);

    //buff[2] == ndef len
    nfc_data_len = buff[2];

    if(nfc_data_len <= 0)
        return NULL;
    
    nfc_data = malloc(nfc_data_len + 1);
    if(!nfc_data)
        return NULL;

    memset(nfc_data, 0, nfc_data_len);

    ptr = nfc_data;

    block_count = nfc_data_len / 16;
    remnant = nfc_data_len % 16;

    do {
        read_ndef[4] += block_size;

        if(block_count > 0) {
            block_size = 16;
        } else {
            block_size = remnant;
            remnant = 0;
        }

        read_ndef[5] = block_size;

        // Last two bytes is for calculated CRC value
        crc_calc(read_ndef, sizeof(read_ndef) - 2);
        nfc_i2c_write(read_ndef, sizeof(read_ndef));
        nfc_i2c_read(buff, block_size + 5);

        memcpy(nfc_data, buff + 2, block_size);
        nfc_data += block_size;
    } while(block_count-- > 0 || remnant);

    nfc_i2c_write(deselect, sizeof(deselect));

    // read response
    nfc_i2c_read(buff, 10);

    return ptr;
}

void nfc_write(void)
{
    uint8_t buff[20];

    uint8_t data[] = {
        0x02, 0x00, 0xD6, 0x00, 0x00, 0x08, 
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00
    };

    int i;

    nfc_i2c_write(&get_i2c_session, sizeof(get_i2c_session));
    nfc_i2c_write(select, sizeof(select));

    crc_calc(select_ndef, sizeof(select_ndef) - 2);
    nfc_i2c_write(select_ndef, sizeof(select_ndef));

    crc_calc(data, sizeof(data) - 2);
    nfc_i2c_write(data, sizeof(data));

    // read response
    nfc_i2c_read(buff, 10);

    for(i = 0; i < 10; i++) {
        printf("%02x ", buff[i]);
    }
    printf("\n");

    nfc_i2c_write(deselect, sizeof(deselect));

    // read response
    nfc_i2c_read(buff, 10);
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
    nfc_i2c_read(buff, 10);
}

void tsens(void *pvParameters)
{
    int i;
    int len = 0;

    i2c_init(5, 4);

    vTaskDelay(5 * 1000 / portTICK_RATE_MS);

    for(i = 0; i < 255; i++) {
        i2c_start();
        if(i2c_write(i)) {
            printf("ack %02x\n", i);
        }
        i2c_stop();
    }
    vTaskDelay(100000 * 1000 / portTICK_RATE_MS);
    
    
    i2c_start();
    if(!i2c_write(0x90)) {
        printf("NO ACK during addr write\n");
    }

    if(!i2c_write(0)) {
        printf("NO ACK at write\n");
    }

    //i2c_stop();

    ///
    i2c_start();
    if(!i2c_write(0x91))
        printf("NO ACK at write\n");

    len = 2;
    printf("TEMP - ");
    while(len--)
        printf("%02x ",  i2c_read(0));

    printf("DONE\n");

    i2c_stop();

    vTaskDelay(100000 * 1000 / portTICK_RATE_MS);
}

void gpio02_interrupt_handler(void) 
{ 
    printf("Interrupt RXed 02!!!!\n"); 
    //printf("Reading from NFC ...[%s]\n", nfc_tag_isr());
}

void gpio_intr(void *pvParameters)
{
    vTaskDelay(5 * 1000 / portTICK_RATE_MS);
    printf("NFC GPIO CFG\n");
    nfc_gpio_cfg();
    //nfc_write();
    vTaskDelay(100000 * 1000 / portTICK_RATE_MS);
}


void user_init(void)
{
    sdk_uart_div_modify(0, UART_CLK_FREQ / 115200);

    i2c_init(5, 4);

    gpio_enable(GPIO_CONFIG, GPIO_INPUT);
    if(!gpio_read(GPIO_CONFIG)) {
        printf("Entering configuration mode...\n");
        configure_wifi_ap();
    } else {
        printf("Entering operational mode...\n");
        configure_wifi_station();
        printf("configuring GPIO02 as interrupt\n");
        gpio_set_interrupt(2, GPIO_INTTYPE_EDGE_NEG);

        xTaskCreate(gpio_intr, (signed char *)"gpio_intr", 2048, NULL, 2, NULL);
        //xTaskCreate(tsens, (signed char *)"tsens", 2048, NULL, 2, NULL);
        //xTaskCreate(nfc_tag, (signed char *)"nfc_tag", 2048, NULL, 2, NULL);
        //xTaskCreate(net_console, (signed char *)"net_console", 2048, NULL, 2, NULL);
        //xTaskCreate(core, (signed char *)"core", 256, NULL, 2, NULL);
    }
}
