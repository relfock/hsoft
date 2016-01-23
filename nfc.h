
bool nfc_i2c_read(uint8_t *buff, int len);
bool nfc_i2c_write(uint8_t *buff, int len);
uint16_t crc_calc(uint8_t *data, uint8_t len);
