#include <i2c/i2c.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>

#include "nfc.h"

#define BUF_SIZE 30

xQueueHandle nfcq;
void configure_wifi_station(void);
void sdk_system_restart(void);

void gpio00_interrupt_handler(void) 
{ 
    uint8_t tmp = 1;
    xQueueSendToBackFromISR(nfcq, &tmp, NULL);
}

static uint16_t update_crc(uint8_t ch, uint16_t *lpw_crc)
{
    ch = (ch ^ (uint8_t)((*lpw_crc) & 0x00FF));
    ch = (ch ^ (ch << 4));
    *lpw_crc = (*lpw_crc >> 8) ^ ((uint16_t)ch << 8) ^
        ((uint16_t)ch<<3) ^ ((uint16_t)ch >> 4);

    return(*lpw_crc);
}

uint16_t crc_calc(uint8_t *data, uint8_t len)
{
    uint8_t ch_block;
    uint16_t w_crc;

    w_crc = 0x6363;

    do {
        ch_block = *data++;
        update_crc(ch_block, &w_crc);
    } while (--len);

    *data = w_crc & 0x00FF;
    *(data + 1) = w_crc >> 8;

    return w_crc;
}

bool nfc_i2c_read(uint8_t *buff, int len)
{
    int i = 0;

    memset(buff, 0, len);

    i2c_start();
    if(!i2c_write(0xAD)) {
        printf("NO ACK at write\n");
        i2c_stop();
        return false;
    }

    while(len--)
        buff[i++] = i2c_read(0);

    i2c_stop();

    return true;
}

bool nfc_i2c_write(uint8_t *buff, int len)
{
    int i = 0;

    i2c_start();
    if(!i2c_write(0xAC)) {
        printf("NO ACK during addr write\n");
        i2c_stop();
        return false;
    }

    while(len--) {
        if(!i2c_write(buff[i++])) {
            printf("NO ACK at write %d\n", i);
            i2c_stop();
            return false;
        }
    }

    i2c_stop();

    return true;
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
    
    nfc_data = malloc(nfc_data_len + 1);
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
            block_size = remnant + 2;
            remnant = 0;
        }

        read_ndef[5] = block_size;

        crc_calc(read_ndef, sizeof(read_ndef) - 2);
        nfc_i2c_write(read_ndef, sizeof(read_ndef));
        nfc_i2c_read(buff, block_size + 7);

        //{ int i; for(i = 0; i < block_size + 7; i++) { printf("%02x ", buff[i]); } printf("\n"); }

        memcpy(nfc_data, buff + 1, block_size);
        nfc_data += block_size;
    } while(block_count-- > 0 || remnant);

    nfc_i2c_write(deselect, sizeof(deselect));

    // read response
    nfc_i2c_read(buff, 3);

    return ptr;
}

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

void nfc_gpio_cfg(void)
{
    uint8_t buff[BUF_SIZE];

    crc_calc(verify, sizeof(verify) - 2);
    crc_calc(select, sizeof(select) - 2);
    crc_calc(select_ndef, sizeof(select_ndef) - 2);
    crc_calc(select_system, sizeof(select_system) - 2);
    crc_calc(read_gpio_cfg, sizeof(read_gpio_cfg) - 2);
    crc_calc(write_gpio_cfg, sizeof(write_gpio_cfg) - 2);

    nfc_i2c_write(&get_i2c_session, sizeof(get_i2c_session));
    nfc_i2c_write(select, sizeof(select));
    nfc_i2c_write(select_ndef, sizeof(select_ndef));
    nfc_i2c_write(select_system, sizeof(select_system));
    nfc_i2c_write(verify, sizeof(verify));

    //nfc_i2c_read(buff, 16); { int i; for(i = 0; i < 16; i++) { printf("%02x ", buff[i]); } printf("\n"); }

    nfc_i2c_write(write_gpio_cfg, sizeof(write_gpio_cfg));
    nfc_i2c_write(read_gpio_cfg, sizeof(read_gpio_cfg));

    // read response
    nfc_i2c_read(buff, 5);

    printf("NFC TAG GPIO config 0x%x\n", buff[1]);

    nfc_i2c_write(deselect, sizeof(deselect));

    // read response
    nfc_i2c_read(buff, 3);
}

void nfc_task(void *pvParameters)
{
    xQueueHandle *nfcq = (xQueueHandle *)pvParameters;

    vTaskDelay(2 * 1000 / portTICK_RATE_MS);

    printf("Waiting for NFC interrupt on gpio 0...\n");
    gpio_set_interrupt(0, GPIO_INTTYPE_EDGE_NEG);

    //nfc_tag_format();
    nfc_gpio_cfg();

    for(;;) {
        uint8_t irq_ts;
        xQueueReceive(*nfcq, &irq_ts, portMAX_DELAY);
        irq_ts *= portTICK_RATE_MS;

        printf("INT RXed at %d: Waiting for RF to write the data...", irq_ts);
        while(!gpio_read(0));
        printf("DONE\n");
        vTaskDelay(1 * 1000 / portTICK_RATE_MS);
        configure_wifi_station();
        sdk_system_restart();
    }
}
