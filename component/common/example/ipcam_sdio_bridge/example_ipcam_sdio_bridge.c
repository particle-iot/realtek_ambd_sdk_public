#include "FreeRTOS.h"
#include "task.h"
#include "spdio_api.h"
#include "platform_stdlib.h"
#include "netif/bridgeif.h"
#include "lwip_netconf.h"
#include <lwip_intf.h>

#include "wifi_constants.h"
#include "wifi_structures.h"
#include "wifi_conf.h"
#include "log_service.h"
#include "sockets.h"


#define IPCAM_SPDIO_TX_BD_NUM	24
#define IPCAM_SPDIO_RX_BD_NUM	20
#define IPCAM_SPDIO_RX_BUFSZ	(SPDIO_RX_BUFSZ_ALIGN(2048+24)) //n*64, must be rounded to 64, extra 24 bytes for spdio header info;

#define SERVER_IP 	"192.168.0.100"
#define SERVER_PORT 5001

extern void bridgeif_set_mac_init(char *bmac);
extern void bridgeif_add_port_sta_netif(void);

#if CONFIG_EXAMPLE_WLAN_FAST_CONNECT
typedef int (*init_done_ptr)(void);
extern init_done_ptr p_wlan_init_done_callback;
extern int wlan_init_done_callback();
#endif

struct spdio_t ipcam_spdio_dev;

#if CONFIG_LWIP_LAYER
extern struct netif xnetif[NET_IF_NUM]; 
#endif

char rx_done_flag = 0;

struct spdio_buf_t* spdio_buf_alloc(u16 size)
{
	struct spdio_buf_t *buf = (struct spdio_buf_t *)malloc(sizeof(struct spdio_buf_t));
	if (!buf)
		return NULL;
	
	memset((u8 *)buf, 0, sizeof(struct spdio_buf_t));
	
	buf->buf_allocated = (u32)malloc(size + SPDIO_DMA_ALIGN_4);
	if (!buf->buf_allocated) {
		free((u8 *)buf);
		return NULL;
	}

	buf->size_allocated = size + SPDIO_DMA_ALIGN_4;
	buf->buf_addr = (u32)N_BYTE_ALIGMENT((u32)(buf->buf_allocated), SPDIO_DMA_ALIGN_4);

	return buf;
}

void spdio_buf_free(struct spdio_buf_t *buf)
{
	free((u8 *)buf->buf_allocated);
	free((u8 *)buf);
}

void print_data(u8 *data, u16 size)
{
	int i = 0;
	
	for(i = 0; i < size; i++)
		printf("0x%02x ",*(char *)(data + i));

	printf("\n");
}

err_t spdio_wlan_send(struct eth_drv_sg *sg_list, int sg_len, u16 size)
{
	struct eth_drv_sg *last_sg;
	struct spdio_buf_t *tx_buf;

	tx_buf = spdio_buf_alloc(size);

	if (!tx_buf)
		return ERR_MEM;

	// copy data
	int offset = 0;
	for(last_sg = &sg_list[sg_len]; sg_list < last_sg; ++sg_list) {
		rtw_memcpy((void *)(tx_buf->buf_addr + offset), (void *)sg_list->buf, sg_list->len);
		offset += sg_list->len;
	}

	tx_buf->buf_size = size;
	tx_buf->type = SPDIO_TX_DATA_ETH; // you can define your own data type in spdio_rx_data_t and spdio_tx_data_t

	//printf("loopback package, size = %d heap=%d\n", size, xPortGetFreeHeapSize());

	if(spdio_tx(&ipcam_spdio_dev, tx_buf) != _TRUE) {
		spdio_buf_free(tx_buf);
	}
	return ERR_OK;
}

void spdio_wlan_recv(u8 *pdata, struct eth_drv_sg *sg_list, int sg_len)
{
	struct eth_drv_sg *last_sg;
	
	for (last_sg = &sg_list[sg_len]; sg_list < last_sg; ++sg_list) {
		if (sg_list->buf != 0) {
			rtw_memcpy((void *)(sg_list->buf), pdata, sg_list->len);
			pdata = (u8 *)(((unsigned int)pdata) + sg_list->len);
		}
	}
}


// send ethernet packet to sdio host
static err_t ipcam_spdio_wlan_tx(struct netif *netif, struct pbuf *p)
{
	struct eth_drv_sg sg_list[MAX_ETH_DRV_SG];
	int sg_len = 0;
	struct pbuf *q;

	if(!rx_done_flag)
		return ERR_OK;


	for(q = p; q != NULL & sg_len < MAX_ETH_DRV_SG; q = q->next) {
		sg_list[sg_len].buf = (u32)q->payload;
		sg_list[sg_len++].len = q->len;
	}

	if(sg_len) {
		return spdio_wlan_send(sg_list, sg_len, p->tot_len);
	}
	return ERR_OK;
}

// send cmd response to host
err_t ipcam_spdio_local_tx(u8* pdata, u16 size)
{
	struct spdio_buf_t *tx_buf;

	tx_buf = spdio_buf_alloc(size);

	if (!tx_buf)
		return ERR_MEM;

	rtw_memcpy((void *)tx_buf->buf_addr, pdata, size);

	tx_buf->buf_size = size;
	tx_buf->type = SPDIO_TX_DATA_ATCMDRSP;

	if(spdio_tx(&ipcam_spdio_dev, tx_buf) != _TRUE) {
		spdio_buf_free(tx_buf);
	}
	return ERR_OK;
}


void spdio_h2c_cmd_handler(struct spdio_t *obj, u8 *pcmd, u16 size)
{
	log_act_t action = NULL;
	char buf[LOG_SERVICE_BUFLEN];
	memset(buf, 0, LOG_SERVICE_BUFLEN);
	char *copy=buf;
	char *token = NULL;
	char *param = NULL;
	char tok[5] = {0};//'\0'

	token = strtok(copy, "=");
	param = strtok(NULL, "\0");

	if(token && strlen(token) <= 4)
		strncpy(tok, token, strlen(token));

	action = (log_act_t)log_action(tok);
        
	if(action){	
		action(param);
	} 
}

void spdio_h2c_data_handler(struct spdio_t *obj, u8 *pdata, u16 size)
{
	struct netif *ipcam_netif = &xnetif[1];
	struct eth_drv_sg sg_list[MAX_ETH_DRV_SG];
	struct pbuf *p, *q;
	int sg_len = 0;
	u16 total_len = size; // size include tcpipheader

	if ((total_len > MAX_ETH_MSG) || (total_len < 0))
		total_len = MAX_ETH_MSG;

	p = pbuf_alloc(PBUF_RAW, total_len, PBUF_POOL);

	if(p == NULL)
		return;

	// Create scatter list
	for (q = p; q != NULL && sg_len < MAX_ETH_DRV_SG; q = q->next) {
   		sg_list[sg_len].buf = (unsigned int) q->payload;
		sg_list[sg_len++].len = q->len;
	}

	if (p->if_idx == NETIF_NO_INDEX) {
		p->if_idx = netif_get_index(ipcam_netif);
	}

	spdio_wlan_recv(pdata, sg_list, sg_len);

	// creat pbuf to input lwip
	if(ERR_OK != ipcam_netif->input(p, ipcam_netif)) {
		pbuf_free(p);
	}
}


/*spdio rx done callback (HOST->Device)*/
char ipcam_spdio_rx_done_cb(void *priv, void *pbuffer, u8 *pdata, u16 size, u8 type)
{
	struct spdio_t *obj = (struct spdio_t *)priv;
	struct spdio_buf_t *rx_buf = (struct spdio_buf_t *)pbuffer;
	u8 pkt_rpt_type = rx_buf->type;
	
	if(!rx_done_flag)
		rx_done_flag = 1;
#if 0
	switch(pkt_rpt_type) {
	case SPDIO_RX_DATA_ETH:
		spdio_h2c_data_handler(obj, pdata, size);
		break;
	case SPDIO_RX_DATA_ATCMD:
		spdio_h2c_cmd_handler(obj, pdata, size);
		break;
	default:
		printf("unsupport packet type!\n");
	}
#endif
	spdio_h2c_data_handler(obj, pdata, size);
	
	// manage rx_buf here
	free((char *)rx_buf->buf_allocated);

	// assign new buffer for SPDIO RX
	rx_buf->buf_allocated = (u32)malloc(obj->rx_bd_bufsz + SPDIO_DMA_ALIGN_4);
	rx_buf->size_allocated = obj->rx_bd_bufsz + SPDIO_DMA_ALIGN_4;

	// this buffer must be 4 byte alignment
	rx_buf->buf_addr = (u32)N_BYTE_ALIGMENT((u32)(rx_buf->buf_allocated), SPDIO_DMA_ALIGN_4);

	return SUCCESS;
}

char ipcam_spdio_tx_done_cb(void *priv, void *pbuf){

	struct spdio_buf_t* tx_buf = (struct spdio_buf_t*)pbuf;

	//printf("%s-->\n",__func__);
	spdio_buf_free(tx_buf);
	return SUCCESS;
}

void ipcam_sdio_init()
{
	int i;

	PAD_PullCtrl(_PB_18, GPIO_PuPd_UP);
	PAD_PullCtrl(_PB_19, GPIO_PuPd_UP);
	PAD_PullCtrl(_PB_20, GPIO_PuPd_UP);
	PAD_PullCtrl(_PB_21, GPIO_PuPd_NOPULL);
	PAD_PullCtrl(_PB_22, GPIO_PuPd_UP);
	PAD_PullCtrl(_PB_23, GPIO_PuPd_UP);
	Pinmux_Config(_PB_18, PINMUX_FUNCTION_SDIOD);
	Pinmux_Config(_PB_19, PINMUX_FUNCTION_SDIOD);
	Pinmux_Config(_PB_20, PINMUX_FUNCTION_SDIOD);
	Pinmux_Config(_PB_21, PINMUX_FUNCTION_SDIOD);
	Pinmux_Config(_PB_22, PINMUX_FUNCTION_SDIOD);
	Pinmux_Config(_PB_23, PINMUX_FUNCTION_SDIOD);

	ipcam_spdio_dev.priv = NULL;
	ipcam_spdio_dev.rx_bd_num = IPCAM_SPDIO_RX_BD_NUM;
	ipcam_spdio_dev.tx_bd_num = IPCAM_SPDIO_TX_BD_NUM;
	ipcam_spdio_dev.rx_bd_bufsz = IPCAM_SPDIO_RX_BUFSZ;

	ipcam_spdio_dev.rx_buf = (struct spdio_buf_t *)malloc(ipcam_spdio_dev.rx_bd_num * sizeof(struct spdio_buf_t));
	if (!ipcam_spdio_dev.rx_buf) {
		printf("malloc failed for spdio buffer structure!\n");
		return;
	}

	for (i = 0; i < ipcam_spdio_dev.rx_bd_num; i++) {
		ipcam_spdio_dev.rx_buf[i].buf_allocated = (u32)malloc(ipcam_spdio_dev.rx_bd_bufsz + SPDIO_DMA_ALIGN_4);
		if (!ipcam_spdio_dev.rx_buf[i].buf_allocated) {
			printf("malloc failed for spdio buffer!\n");
			return;
		}
		ipcam_spdio_dev.rx_buf[i].size_allocated = ipcam_spdio_dev.rx_bd_bufsz + SPDIO_DMA_ALIGN_4;
		// this buffer must be 4 byte alignment
		ipcam_spdio_dev.rx_buf[i].buf_addr = (u32)N_BYTE_ALIGMENT((u32)(ipcam_spdio_dev.rx_buf[i].buf_allocated), SPDIO_DMA_ALIGN_4);
	}

	ipcam_spdio_dev.rx_done_cb = ipcam_spdio_rx_done_cb;
	ipcam_spdio_dev.tx_done_cb = ipcam_spdio_tx_done_cb;

	spdio_init(&ipcam_spdio_dev);
}

int connect_to_server(char* ip,int port)
{
	int client_fd = -1;
	int socket_ruse = 1;
	struct sockaddr_in	svr_addr;

	rtw_memset(&svr_addr, 0, sizeof(svr_addr));
	svr_addr.sin_family = AF_INET;
	svr_addr.sin_port = htons(port);
	svr_addr.sin_addr.s_addr = inet_addr(ip);

	/* Connecting to server */
	if((client_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		printf("[ERROR] %s: TCP client create error",__func__);
		goto Error;
	}
	printf("==>%s: Create socket fd = %d", __func__, client_fd);
	printf("==>%s: Connect to server %s:%d",__func__, ip, port);
	setsockopt(client_fd, SOL_SOCKET, SO_REUSEADDR, (char *)&socket_ruse, sizeof(socket_ruse));
	if(connect(client_fd, (struct sockaddr*)&svr_addr, sizeof(svr_addr)) < 0){
		printf("[ERROR] %s: TCP client connect error",__func__);
		goto Error;
	}
	printf("==>%s: Connect to server success !",__func__);
	return client_fd;
Error:	
	close(client_fd);
	client_fd = -1;
	return client_fd;	
}

int send_to_server(int fd,char *data)
{
    int data_size = strlen(data);
	char data_size_buf[4] = {0};

	data_size_buf[0] = (data_size & 0xFF);
	data_size_buf[1] = (data_size & 0xFFFF) >> 8;
	data_size_buf[2] = (data_size & 0xFFFFFF) >> 16;
	data_size_buf[3] = (data_size & 0xFFFFFFFF) >> 24;
	
	if(send(fd, data_size_buf, 4, 0) <= 0) {
		printf("[ERROR] %s: TCP client send data error",__func__);
		return -1;
	}

	if(send(fd, data, data_size, 0) <= 0) {
		printf("[ERROR] %s: TCP client send data error",__func__);
		return -1;
	}
	return 0;
}


void ipcam_heartbeat_thread(void * param)
{
	int fd = -1;
	u8 buf[128] = "hello world";

	_sema hb_sema;
	
	if(wifi_is_ready_to_transceive(0) != 0){
		printf("wifi disconnected !");
		vTaskDelay(2000);
	}

	rtw_init_sema(&hb_sema, 0);

RECONNECT:
	fd = connect_to_server(SERVER_IP, SERVER_PORT);
	if(fd < 0)
		goto RECONNECT;

	while(1) {
		send_to_server(fd, buf);
		printf("==>>>>> HeartBeat <<<<<==");
		if(1 == rtw_down_timeout_sema(&hb_sema, 5000)) {
			rtw_msleep_os(100);
			printf("\r\n==>>>>> Reconnect <<<<<==\r\n\n");
			goto RECONNECT;
		}
	}

	vTaskDelete(NULL);
}


void ipcam_sdio_bridge_thread(void *param)
{
	// wait wifi connect complete in uart atcmd thread
	// comment if needed 
	vTaskDelay(3000);

	wifi_off();
	vTaskDelay(20);

	/*********************************************************************************
	*	1-2. Enable Wi-Fi with STA + AP mode
	*********************************************************************************/
	printf("\n\rEnable Wi-Fi with STA + AP mode\n");
	if(wifi_on(RTW_MODE_STA_AP) < 0){
		printf("\n\rERROR: wifi_on failed\n");
		return;
	}

	u8 *mac = LwIP_GetMAC(&xnetif[0]);

	struct ip_addr ipaddr;
	struct ip_addr netmask;
	struct ip_addr gw;

	IP4_ADDR(&ipaddr, 10, 0, 0, 3);  // can modify to link local ip 169.254.x.x
	IP4_ADDR(&netmask, 255, 255, 255, 0);
	IP4_ADDR(&gw, 10, 0, 0, 1);

	bridgeif_set_mac_init(mac);
	bridgeif_add_port_sta_netif();

	// add a virtual net interface to represent ip camera
	struct netif *ipcam_netif = &xnetif[1];
	memcpy(ipcam_netif->hwaddr, mac, ETH_HWADDR_LEN);
	
	netif_set_addr(ipcam_netif, &ipaddr, &netmask, &gw);
	ipcam_netif->linkoutput = ipcam_spdio_wlan_tx;
	bridgeif_add_port(&xnetif[NET_IF_NUM - 1], ipcam_netif);

	wext_set_mode(ipcam_netif, RTW_MODE_MASTER);
	netif_set_flags(ipcam_netif, NETIF_FLAG_ETHARP);
	netif_set_link_up(ipcam_netif);

	ipcam_sdio_init();
	printf("SDIO device starts!\n");

	printf("\n\rConnect to AP\n");
	if(p_wlan_init_done_callback)
		p_wlan_init_done_callback();

	//if(xTaskCreate(ipcam_heartbeat_thread, ((const char*)"ipcam_heartbeat_thread"), 1024, NULL, tskIDLE_PRIORITY + 3, NULL) != pdPASS) {
	//	printf("xTaskCreate(ex_spdio_thread) failed\r\n");
	//}

	vTaskDelete(NULL);
}

int example_ipcam(void)
{
	if(xTaskCreate(ipcam_sdio_bridge_thread, ((const char*)"ipcam_pdio_thread"), 2048, NULL, tskIDLE_PRIORITY + 5, NULL) != pdPASS) {
		printf("xTaskCreate(ex_spdio_thread) failed\r\n");
	}

	return 0;
}

