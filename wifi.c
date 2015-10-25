#include <string.h>
#include <lwip/api.h>
#include <dhcpserver.h>

#include <espressif/esp_common.h>
#include <espressif/sdk_private.h>

#define AP_SSID "Addon1"
#define AP_PASSWD "ad123456"

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

void configure_wifi_station(void)
{
    struct sdk_station_config st_config = {
        .ssid = "relfock-main",
        .password = "tpandrew",
    };

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_station_set_config(&st_config);

    if(0 == sdk_wifi_station_get_auto_connect()) {
        printf("Wifi auto connect is disabled: Enabling...\n");
        if(!sdk_wifi_station_set_auto_connect(1))
            printf("sdk_wifi_station_set_auto_connect: FAILED\n");
    }

    return;
}
