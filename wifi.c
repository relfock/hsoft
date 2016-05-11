#include <string.h>
#include <lwip/api.h>
#include <dhcpserver.h>

#include <espressif/esp_common.h>
#include <espressif/sdk_private.h>

#define AP_SSID "Addon1"
#define AP_PASSWD "ad123456"

void nfc_tag_format(void);
extern char server_port[16];
extern char server_ip[512];

void configure_wifi_ap(void)
{
    struct ip_info ap_ip;

    struct sdk_softap_config ap_config = {
        .ssid = AP_SSID,
        .ssid_hidden = 0,
        .channel = 3,
        .ssid_len = strlen(AP_SSID),
        .authmode = AUTH_WPA_WPA2_PSK,
        .password = AP_PASSWD,
        .max_connection = 3,
        .beacon_interval = 100,
    };

    IP4_ADDR(&ap_ip.ip, 192, 168, 10, 1);
    IP4_ADDR(&ap_ip.gw, 0, 0, 0, 0);
    IP4_ADDR(&ap_ip.netmask, 255, 255, 0, 0);

    sdk_wifi_set_opmode(SOFTAP_MODE);
    sdk_wifi_set_ip_info(1, &ap_ip);
    sdk_wifi_softap_set_config(&ap_config);

    ip_addr_t first_client_ip;
    IP4_ADDR(&first_client_ip, 192, 168, 10, 2);
    dhcpserver_start(&first_client_ip, 4);

    return;
}

uint8_t* nfc_tag_read(void);

int get_prop(char *str, char *prop, char *cfg)
{
    char *start = NULL, *end = NULL;

    start = strstr(str, prop);
    if(!start) {
        return -1;
    }

    start += strlen(prop);

    end = index(start, ':');
    if(!end) {
        return -1;
    }

    strncpy((char*)cfg, start, end - start);

    return 0;
}

void configure_wifi_station(void)
{
    char *ucfg = NULL;
    struct ip_info static_ip;
    int ip[4], mask[4], gw[4];
    char ip_str[32], mask_str[32], gw_str[32];
    struct sdk_station_config st_config;

    memset(ip_str, 0, sizeof(ip_str));
    memset(gw_str, 0, sizeof(gw_str));
    memset(mask_str, 0, sizeof(mask_str));
    memset(st_config.ssid, 0, 32);
    memset(st_config.password, 0, 64);

    ucfg = (char*)nfc_tag_read();
    if(!ucfg) {
        printf("%s:%d Failed to get user configuration from NFC\n", 
                __func__, __LINE__);
        nfc_tag_format();
        return;
    }

    if(get_prop(ucfg + 9, "ssid=", (char*)st_config.ssid) != 0 ||
            get_prop(ucfg + 9, "pwd=", (char*)st_config.password) != 0) {
        printf("%s:%d Failed to get correct user configuration from NFC\n", 
                __func__, __LINE__);
        nfc_tag_format();
        return;
    }

    printf("\nNFC INFO: [%s]\n", ucfg + 9);
    printf("Configuring WIFI: SSID[%s] PASSWD[%s]\n", st_config.ssid, st_config.password);

    // Get server cfg
    memset(server_ip, 0, sizeof(server_ip));
    memset(server_port, 0, sizeof(server_port));

    get_prop(ucfg + 9, "srv=", server_ip);
    get_prop(ucfg + 9, "port=", server_port);

    if(get_prop(ucfg + 9, "ip=", ip_str) == 0 &&
            get_prop(ucfg + 9, "mask=", mask_str) == 0 &&
            get_prop(ucfg + 9, "gw=", gw_str) == 0) {

        if(sscanf(ip_str, "%d.%d.%d.%d", &ip[0], &ip[1], &ip[2], &ip[3]) != 4 ||
                sscanf(gw_str, "%d.%d.%d.%d", &gw[0], &gw[1], &gw[2], &gw[3]) != 4 ||
                sscanf(mask_str, "%d.%d.%d.%d", &mask[0], &mask[1], &mask[2], &mask[3]) != 4) {
            printf("Wrong static IP configuraton: [%s], [%s], [%s]\n", ip_str, gw_str, mask_str);
            goto dhcp;
        }

        printf("Using static configuraton\n");
        printf("Static IP configuraton: [%s], [%s], [%s]\n", ip_str, gw_str, mask_str);
    } else {
dhcp:
        memset(ip, 0, sizeof(ip));
        memset(gw, 0, sizeof(gw));
        memset(mask, 0, sizeof(mask));
        printf("Using dynamic configuraton\n");
    }

    IP4_ADDR(&static_ip.ip, ip[0], ip[1], ip[2], ip[3]);
    IP4_ADDR(&static_ip.gw, gw[0], gw[1], gw[2], gw[3]);
    IP4_ADDR(&static_ip.netmask, mask[0], mask[1], mask[2], mask[3]);
    sdk_wifi_set_ip_info(0, &static_ip);

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&st_config);

    free(ucfg);

    return;
}
