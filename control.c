

//#define TELNET_PORT 23
//
//    struct netconn *client = NULL;
//    err_t err;
//
//
//    //TELNET
//    {
//        struct netconn *nc = netconn_new (NETCONN_TCP);
//        if(!nc) {
//            printf("Status monitor: Failed to allocate socket.\r\n");
//            return;
//        }
//        netconn_bind(nc, IP_ADDR_ANY, TELNET_PORT);
//        netconn_listen(nc);
//
//reconnect:
//        if(client)
//            netconn_close(client);
//accept:
//        err = netconn_accept(nc, &client);
//
//        if ( err != ERR_OK ) {
//            if(client)
//                netconn_delete(client);
//            goto accept;
//        }
//
//        snprintf(buf, sizeof(buf), "Uptime %d seconds\r\n", xTaskGetTickCount()*portTICK_RATE_MS/1000);
//        if(netconn_write(client, buf, strlen(buf), NETCONN_COPY) < 0)
//            goto reconnect;
//    }
//
//        snprintf(buf, sizeof(buf), "Voltage[%d]\r\n", volt);
//        if(netconn_write(client, buf, strlen(buf), NETCONN_COPY) < 0)
//            goto reconnect;
