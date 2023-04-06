#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBH_CDC_ACM) && CONFIG_EXAMPLE_USBH_CDC_ACM
#include <platform/platform_stdlib.h>
#include "osdep_service.h"
#include "usb.h"
#include "usbh_cdc_acm_if.h"
#include "dwc_otg_driver.h"

#define CONFIG_USBH_CDC_ACM_MEMORY_LEAK_CHECK 0  /*test for hotplug memory leak*/
#define STRESS_TEST 0 /*long run test*/

#define ACM_LOOPBACK_BULK_SIZE 8192  /*it should match with device loopback buffer size*/
#define LOOPBACK_TIMES  100

u8 acm_loopback_tx_buf[ACM_LOOPBACK_BULK_SIZE];
u8 acm_loopback_rx_buf[ACM_LOOPBACK_BULK_SIZE];

_sema CDCSema;
_sema TestSema;
_sema LoopbackSema;
volatile int TotalRxLen = 0;

void cdc_loopback_test(void)
{
	int i, j, k;
	int ret;
	u32 start,end;

	for(i = 0; i < ACM_LOOPBACK_BULK_SIZE; i++)
		acm_loopback_tx_buf[i] = i%128;
	
	printf("\nloopback test begin, loopback times:%d, size: %d\n", LOOPBACK_TIMES, ACM_LOOPBACK_BULK_SIZE);
	
#if STRESS_TEST
	while(1){
#endif
	for( i = 0; i < LOOPBACK_TIMES; i++){ 
		memset(acm_loopback_rx_buf, 0 , ACM_LOOPBACK_BULK_SIZE);
		if(!usbh_get_connect_status()){
			printf("disconnected!!");
			return;
		}
		ret = usbh_cdc_acm_write( acm_loopback_tx_buf, ACM_LOOPBACK_BULK_SIZE);
		if(ret < 0){
			printf("write fail: %d\n", ret);
			return;
		}

		if(rtw_down_sema(&LoopbackSema)){
			/*check rx data*/
			for( k =0 ; k< ACM_LOOPBACK_BULK_SIZE; k++){
				if(acm_loopback_rx_buf[i] != i%128){
					printf("loopback test fail:%d, %d, %d\n", i, i%128, acm_loopback_rx_buf[i]);
					for(int j = i ; j< i + 200; j++){
						printf("%d ",acm_loopback_rx_buf[j]);
					}
					return;
				}
			}
		}
	}
	printf("loopback test success\n");
#if STRESS_TEST
	}
#endif
}


void cdc_request_test(void)
{
	int ret;
	u32 baudrate1, baudrate2;
	CDC_LineCodingTypeDef LCStruct;
	
	printf("\nrequest test begin\n");
	
	ret = usbh_cdc_acm_set_ctrl_line_state(CDC_ACTIVATE_SIGNAL_DTR | CDC_ACTIVATE_CARRIER_SIGNAL_RTS);
	if(ret < 0){
		printf("set_ctrl_line_state request fail\n");
	}
	
	usbh_cdc_acm_get_line_coding(&LCStruct);
	if(ret < 0){
		printf("get_line_coding request fail\n");
	}
	baudrate1 = LCStruct.b.dwDTERate;

	LCStruct.b.dwDTERate = 38400;
	LCStruct.b.bCharFormat= USB_CDC_1_STOP_BITS;
	LCStruct.b.bParityType = USB_CDC_NO_PARITY;
	LCStruct.b.bDataBits = 8;
	usbh_cdc_acm_set_line_coding(&LCStruct);
	if(ret < 0){
		printf("set_line_coding request fail\n");
	}
	memset(&LCStruct, 0, LINE_CODING_STRUCTURE_SIZE);
	
	usbh_cdc_acm_get_line_coding(&LCStruct);
	if(ret < 0){
		printf("get_line_coding request fail\n");
	}
	baudrate2 = LCStruct.b.dwDTERate;

	printf("baudrate: before %d, set %d, after %d\n", baudrate1, 38400, baudrate2);
	printf("request test success\n");
}

static void cdc_acm_init(void)
{
	printf("\nINIT\n");
}

static void cdc_acm_deinit(void)
{
	printf("\nDEINIT\n");
}

static void cdc_acm_attach(void)
{
	printf("\nATTACH\n");
	rtw_up_sema(&TestSema);
}

#if CONFIG_USBH_CDC_ACM_MEMORY_LEAK_CHECK
static void cdc_acm_detach(void)
{
	printf("\nDETACH\n");
	rtw_up_sema(&CDCSema);
}
#endif

static void cdc_acm_receive(u8 *buf, u32 length)
{
	memcpy(acm_loopback_rx_buf+TotalRxLen, buf, length);
	if((TotalRxLen +length) >= ACM_LOOPBACK_BULK_SIZE){
		TotalRxLen = 0;
		rtw_up_sema(&LoopbackSema);
	}else{
		TotalRxLen += length;
	}
}


usbh_cdc_acm_usr_cb_t cdc_acm_usr_cb = {
	.init = cdc_acm_init,
	.deinit = cdc_acm_deinit,
	.attach = cdc_acm_attach,
#if CONFIG_USBH_CDC_ACM_MEMORY_LEAK_CHECK
	.detach = cdc_acm_detach,
#endif
	.receive = cdc_acm_receive,
};

#if CONFIG_USBH_CDC_ACM_MEMORY_LEAK_CHECK
static void cdc_acm_check_memory_leak_thread(void *param)
{
	int ret = 0;

	UNUSED(param);

	for (;;) {
		rtw_down_sema(&CDCSema);
		rtw_mdelay_os(100);//make sure disconnect handle finish before deinit.
		usbh_cdc_acm_deinit();
		usb_deinit();
		rtw_mdelay_os(10);
		printf("FreeHeapSize:%x\n", xPortGetFreeHeapSize());
		
		ret = usb_init();
		if (ret != USB_INIT_OK) {
			printf("\nFail to init USB host controller: %d\n", ret);
			break;
		}

		ret = usbh_cdc_acm_init(&cdc_acm_usr_cb);
		if (ret < 0) {
			printf("\nFail to init USB host cdc_acm driver: %d\n", ret);
			usb_deinit();
			break;
		}
	}

	rtw_thread_exit();
}
#endif

void example_usbh_cdc_acm_thread(void *param)
{
	int status;
	struct task_struct task;

	UNUSED(param);
	rtw_init_sema(&CDCSema, 0);
	rtw_init_sema(&TestSema, 0);
	rtw_init_sema(&LoopbackSema, 0);
	status = usb_init();
	if (status != USB_INIT_OK) {
		printf("\nFail to init USB host controller: %d\n", status);
		goto error_exit;
	}

	status = usbh_cdc_acm_init(0, 0, &cdc_acm_usr_cb);  /*0 means use default transfer size, and it can not exceed 65536*/
	if (status < 0) {
		printf("\nFail to init USB host cdc_acm driver: %d\n", status);
		usb_deinit();
		goto error_exit;
	}

#if CONFIG_USBH_CDC_ACM_MEMORY_LEAK_CHECK
	status = rtw_create_task(&task, "cdc_acm_check_memory_leak_thread", 512, tskIDLE_PRIORITY + 3, cdc_acm_check_memory_leak_thread, NULL);
	if (status != pdPASS) {
		printf("\nFail to create USBH cdc_acm memory leak check thread\n");
		usbh_cdc_acm_deinit();
		usb_deinit();
		goto error_exit;
	}
#endif

	if(rtw_down_sema(&TestSema)){
		cdc_request_test();
		cdc_loopback_test();
	}

	goto example_exit;	
error_exit:
	rtw_free_sema(&CDCSema);
	rtw_free_sema(&TestSema);
	rtw_free_sema(&LoopbackSema);
example_exit:	
	rtw_thread_exit();
}


void example_usbh_cdc_acm(void)
{
	int status;
	struct task_struct task;
	printf("\nUSB host cdc_acm demo started...\n");
	status = rtw_create_task(&task, "example_usbh_cdc_acm_thread", 512, tskIDLE_PRIORITY + 1, example_usbh_cdc_acm_thread, NULL);
	if (status != pdPASS) {
		printf("\nFail to create USB host cdc_acm thread: %d\n", status);
	}
}

#endif
