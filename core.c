#include <stdio.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <FreeRTOS.h>
#include <task.h>

#include "esp8266.h"
#include "hsoft.h"
#include "rtc.h"
#include "lwip/sockets.h"
#include <i2c/i2c.h>
 
#define BUFLEN 512  //Max length of buffer

char server_port[16];
char server_ip[512] = { 0 };

void sdk_os_delay_us(uint16_t us);

void core(void *pvParameters)
{
    char message[BUFLEN];
    struct sockaddr_in si_other;
    int s, slen = sizeof(si_other);

    if(server_ip[0] == 0 || server_port[0] == 0) {
        printf("ERROR: No server specified\n");
        vTaskDelay(3000 * 1000 / portTICK_RATE_MS);
    }

    //Wait for WIFI connectivity
    vTaskDelay(5 * 1000 / portTICK_RATE_MS);
 
    if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
        printf("socket: error !!!!!!\n");
        vTaskDelay(3000 * 1000 / portTICK_RATE_MS);
    }
 
    memset((char *) &si_other, 0, sizeof(si_other));
    si_other.sin_family = AF_INET;
    si_other.sin_port = htons(atoi(server_port));
     
    if (inet_aton(server_ip , &si_other.sin_addr) == 0) {
        printf("inet_aton() failed\n");
        vTaskDelay(3000 * 1000 / portTICK_RATE_MS);
    }
 
    memset(message, 0, BUFLEN);

    while(1)
    {
        struct rtc_time t;
        rtc_get(&t);

        int temp;
        uint8_t data[2];
        i2c_slave_read(0x48, 0x00, data, 2);

        temp = ((data[1] >> 7) | ((data[0] & 0x7F) << 1)) / 2;

        if(data[0] & 0x80) 
            temp = -temp;

        sprintf(message, "%02d-%02d-%d %02d:%02d:%02d TEMP[%d]\n", 
                t.tm_mday, t.tm_mon, t.tm_year,
                t.tm_hour, t.tm_min, t.tm_sec, temp);

        printf("sending %s", message);
        //send the message
        if (sendto(s, message, strlen(message), 0, (struct sockaddr *) &si_other, slen) == -1) {
            printf("sendto() failed\n");
        }

        //if (recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen) == -1) {
        //    die("recvfrom()");
        //}
        vTaskDelay(1 * 1000 / portTICK_RATE_MS);
    }

    close(s);
}
