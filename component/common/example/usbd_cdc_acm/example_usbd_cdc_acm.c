#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBD_CDC_ACM) && CONFIG_EXAMPLE_USBD_CDC_ACM
#include <platform_stdlib.h>
#include "usb.h"
#include "usbd_cdc_acm.h"
#include "osdep_service.h"

// This configuration is used to enable a thread to check hotplug event
// and reset USB stack to avoid memory leak, only for example.
// For bus-powered device, it shall be set to 0.
// For self-powered device:
// * it is suggested to check the hotplug event via hardware VBUS GPIO interrupt
//   support other than this software polling thread.
// * while if there is no hardware VBUS GPIO interrupt support, set this configuration
//   to 1 for a better support for hotplug.
// * Set this configuration to 0 to save CPU timeslice for better performance, if
//   there is no need for hotplug support.
#define CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS  1

// USBD CDC ACM applications
#define ACM_APP_ECHO_SYNC                     0     // Echo synchronously in USB IRQ thread, for ACM_BULK_XFER_SIZE <= ACM_BULK_IN_BUF_SIZE
#define ACM_APP_ECHO_ASYNC                    1     // Echo asynchronously in dedicated USB CDC ACM bulk in thread (USBD_CDC_ACM_USE_BULK_IN_THREAD == 1), for ACM_BULK_XFER_SIZE > ACM_BULK_IN_BUF_SIZE
#define ACM_APP_LOOPBACK				2 	//loopback with AmebD usb cdc acm host

#define CONFIG_USDB_CDC_ACM_APP              (ACM_APP_ECHO_SYNC)

// USB speed
#define CONFIG_CDC_ACM_SPEED					USB_SPEED_HIGH

// transfer length should equal to max packet length in ep descriptor because host will not send
// zlp packet after send packet that length equal to max packet length
#if defined(CONFIG_CDC_ACM_SPEED) && (CONFIG_CDC_ACM_SPEED == USB_SPEED_HIGH)
#define ACM_BULK_BUF_SIZE                  USBD_CDC_ACM_HS_BULK_MAX_PACKET_SIZE
#else
#define ACM_BULK_BUF_SIZE                  USBD_CDC_ACM_FS_BULK_MAX_PACKET_SIZE
#endif

#define ACM_BULK_XFER_SIZE                    2048  // Data size to be TX, i.e. the transfer bytes each time we want to send to host via TX API

#define ACM_LOOPBACK_BULK_SIZE	8192  //it should match with host loopback bulk size

#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_ASYNC)
// ACM bulk transfer buffer, which buffers the received bulk out data and transmit back to host.
// It is suggested to set USBD_CDC_ACM_ALLOC_ASYNC_BULK_IN_BUF to 1 to speed up via double buffer, 
// othersize, the buffer shall be protected during bulk in transfer.
static u8 acm_bulk_xfer_buf[ACM_BULK_XFER_SIZE];
static u16 acm_bulk_xfer_buf_pos = 0;
static volatile int acm_bulk_xfer_busy = 0;
#endif


#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_LOOPBACK)
u8 acm_loopback_bulk_buf[ACM_LOOPBACK_BULK_SIZE];
u32 acm_loopback_bulk_pos = 0;
_sema LoopbackTxSema;
struct task_struct loopback_task;
#endif

static int acm_init(void)
{
#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_ASYNC)
    acm_bulk_xfer_buf_pos = 0;
    acm_bulk_xfer_busy = 0;
#endif
    return USB_ESUCCESS;
}

static int acm_deinit(void)
{
#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_ASYNC)
    acm_bulk_xfer_buf_pos = 0;
    acm_bulk_xfer_busy = 0;
#endif
    return USB_ESUCCESS;
}

// This callback function will be invoked in USB IRQ task,
// it is not suggested to do time consuming jobs here.
static int acm_receive(void *buf, u16 length)
{
    int ret = USB_ESUCCESS;
    u16 len = length;
    
#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_SYNC)

	if (len >= ACM_BULK_BUF_SIZE) {
		len = ACM_BULK_BUF_SIZE;  // extra data discarded
    }

    ret = usbd_cdc_acm_sync_transmit_data(buf, len);
    if (ret != USB_ESUCCESS) {
        printf("\nFail to transmit data: %d\n", ret);
    }

#elif (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_ASYNC)

    if (!acm_bulk_xfer_busy) {

        if ((acm_bulk_xfer_buf_pos + len) >= ACM_BULK_XFER_SIZE) {
            len = ACM_BULK_XFER_SIZE - acm_bulk_xfer_buf_pos;  // extra data discarded
        }
        
        rtw_memcpy((void *)((u32)acm_bulk_xfer_buf + acm_bulk_xfer_buf_pos), buf, len);
        acm_bulk_xfer_buf_pos += len;
        if (acm_bulk_xfer_buf_pos >= ACM_BULK_XFER_SIZE) {
            acm_bulk_xfer_buf_pos = 0;
            ret = usbd_cdc_acm_async_transmit_data((void *)acm_bulk_xfer_buf, ACM_BULK_XFER_SIZE);
            if (ret == USB_ESUCCESS) {
                acm_bulk_xfer_busy = 1;
            } else {
                printf("\nFail to transmit data: %d\n", ret);
                if (ret != -USB_EBUSY) {
                    acm_bulk_xfer_busy = 0;
                }
            }
        }
    } else {
        printf("\nBusy, discarded %d bytes\n", length);
        ret = -USB_EBUSY;
    }

#elif (CONFIG_USDB_CDC_ACM_APP == ACM_APP_LOOPBACK)
	rtw_memcpy(acm_loopback_bulk_buf + acm_loopback_bulk_pos, buf, length);
	acm_loopback_bulk_pos += length;
	
	if(acm_loopback_bulk_pos >= ACM_LOOPBACK_BULK_SIZE){
		acm_loopback_bulk_pos = 0;
		rtw_up_sema(&LoopbackTxSema);
	}
   
#endif

    return ret;
}

#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_ASYNC)
static void acm_transmit_complete(int status)
{
    acm_bulk_xfer_busy = 0;
    if (status != USB_ESUCCESS) {
        printf("\nTransmit error: %d\n", status);
    }
}
#endif

usbd_cdc_acm_usr_cb_t cdc_acm_usr_cb = {
    .init = acm_init,
    .deinit = acm_deinit,
    .receive = acm_receive,
#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_ECHO_ASYNC)
    .transmit_complete = acm_transmit_complete,
#endif
};

#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_LOOPBACK)
static void acm_loopback_tx_thread(void *param)
{	
	UNUSED(param);
	int ret;
	u32 tx_len;
	
	while(1){
		if(rtw_down_sema(&LoopbackTxSema)){
			tx_len = ACM_LOOPBACK_BULK_SIZE;
			while(1){
				if (tx_len > ACM_BULK_BUF_SIZE) {
					ret = usbd_cdc_acm_sync_transmit_data(acm_loopback_bulk_buf, ACM_BULK_BUF_SIZE);
					if (ret != USB_ESUCCESS) {
					        printf("\nFail to transmit data: %d\n", ret);
					}
					tx_len -= ACM_BULK_BUF_SIZE;
				}else{
					ret = usbd_cdc_acm_sync_transmit_data(acm_loopback_bulk_buf, tx_len);
					if (ret != USB_ESUCCESS) {
					        printf("\nFail to transmit data: %d\n", ret);
					}
					tx_len = 0;
					break;
				}
			}
		}
	}
}
#endif

    
#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
static void cdc_check_usb_status_thread(void *param)
{
    int ret = 0;
    int usb_status = USB_STATUS_INIT;
    static int old_usb_status = USB_STATUS_INIT;
    
    UNUSED(param);
    
    for (;;) {
        rtw_mdelay_os(100);
        usb_status = usb_get_status();
        if (old_usb_status != usb_status) {
            old_usb_status = usb_status;
            if (usb_status == USB_STATUS_DETACHED) {
                printf("\nUSB DETACHED\n");
#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_LOOPBACK)
                rtw_delete_task(&loopback_task);
                rtw_free_sema(&LoopbackTxSema);
#endif
                usbd_cdc_acm_deinit();
                usb_deinit();
                ret = usb_init(USB_SPEED_HIGH);
                if (ret != 0) {
                    printf("\nFail to re-init USBD driver\n");
                    break;
                }
				ret = usbd_cdc_acm_init(CONFIG_CDC_ACM_SPEED, &cdc_acm_usr_cb);
                if (ret != 0) {
                    printf("\nFail to re-init USB CDC ACM class\n");
                    usb_deinit();
                    break;
                }
#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_LOOPBACK)
                rtw_init_sema(&LoopbackTxSema, 0);
                ret = rtw_create_task(&loopback_task, "acm_loopback_tx_thread", 512, tskIDLE_PRIORITY + 2, acm_loopback_tx_thread, NULL);
                if (ret != pdPASS) {
                    printf("\nFail to create USBD CDC ACM status check thread\n");
                    usbd_cdc_acm_deinit();
                    usb_deinit();
        		break;
                }
#endif
            } else if (usb_status == USB_STATUS_ATTACHED) {
                printf("\nUSB ATTACHED\n");
            } else {
                printf("\nUSB INIT\n");
            }
        }
    }
    
    rtw_thread_exit();
}
#endif // CONFIG_USDB_MSC_CHECK_USB_STATUS

static void example_usbd_cdc_acm_thread(void *param)
{
    int ret = 0;
#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
    struct task_struct task;
#endif
    
    UNUSED(param);
    
	ret = usb_init(CONFIG_CDC_ACM_SPEED);
    if (ret != 0) {
        printf("\nFail to init USBD controller\n");
        goto example_usbd_cdc_acm_thread_fail;
    }

	ret = usbd_cdc_acm_init(CONFIG_CDC_ACM_SPEED, &cdc_acm_usr_cb);
    if (ret != 0) {
        printf("\nFail to init USBD CDC ACM class\n");
        usb_deinit();
        goto example_usbd_cdc_acm_thread_fail;
    }
    
#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
    ret = rtw_create_task(&task, "cdc_check_usb_status_thread", 512, tskIDLE_PRIORITY + 2, cdc_check_usb_status_thread, NULL);
    if (ret != pdPASS) {
        printf("\nFail to create USBD CDC ACM status check thread\n");
        usbd_cdc_acm_deinit();
        usb_deinit();
        goto example_usbd_cdc_acm_thread_fail;
    }
#endif // CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS

#if (CONFIG_USDB_CDC_ACM_APP == ACM_APP_LOOPBACK)
    rtw_init_sema(&LoopbackTxSema, 0);
    ret = rtw_create_task(&loopback_task, "acm_loopback_tx_thread", 512, tskIDLE_PRIORITY + 2, acm_loopback_tx_thread, NULL);
    if (ret != pdPASS) {
        printf("\nFail to create USBD CDC ACM status check thread\n");
        usbd_cdc_acm_deinit();
        usb_deinit();
        goto example_usbd_cdc_acm_thread_fail;
    }
#endif

    rtw_mdelay_os(100);
    
    printf("\nUSBD CDC ACM demo started\n");

example_usbd_cdc_acm_thread_fail:

    rtw_thread_exit();
}

void example_usbd_cdc_acm(void)
{
    int ret;
    struct task_struct task;

    ret = rtw_create_task(&task, "example_usbd_cdc_acm_thread", 1024, tskIDLE_PRIORITY + 5, example_usbd_cdc_acm_thread, NULL);
    if (ret != pdPASS) {
        printf("\nFail to create USBD CDC ACM thread\n");
    }
}

#endif

