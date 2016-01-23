#include <i2c/i2c.h>
#include <stdint.h>
#include <stdio.h>

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

    *(uint16_t*)data = w_crc;

    return w_crc;
}

bool nfc_i2c_read(uint8_t *buff, int len)
{
    int i = 0;

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
