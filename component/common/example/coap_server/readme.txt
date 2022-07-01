COAP SERVER EXAMPLE

Description:
This example demonstrates how to use libcoap C library to build a CoAP server.

Configuration:
Modify SERVER_HOST, SERVER_PORT in example_coap_server.c based on your CoAP server.
[platform_opts.h]
#define CONFIG_EXAMPLE_COAP_SERVER              1

// The configurations below have been added in lwopopts.h 
[lwipopts.h]
#define LWIP_TIMEVAL_PRIVATE            1
#define SO_REUSE                        1
#define MEMP_USE_CUSTOM_POOLS           1
#define LWIP_IPV6                       1

Execution:
Can make automatical Wi-Fi connection when booting by using wlan fast connect example.
A CoAP server example thread will be started automatically when booting.

Note:
Company Firewall may block CoAP message. You can use copper (https://addons.mozilla.org/en-US/firefox/addon/copper-270430/) to test the server's reachability.


Supported IC : 
        Ameba-1, Ameba-z2, AmebaD, 
