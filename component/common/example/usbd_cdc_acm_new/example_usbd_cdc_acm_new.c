/**
  ******************************************************************************
  * @file    example_usbd_cdc_acm_new.c
  * @author  Realsil WLAN5 Team
  * @version V1.0.0
  * @date    2021-6-18
  * @brief   This file is example of usbd cdc acm class
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------ */

#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBD_CDC_ACM) && CONFIG_EXAMPLE_USBD_CDC_ACM
#include "usbd.h"
#include "usbd_cdc_acm.h"
#include "osdep_service.h"

/* Private defines -----------------------------------------------------------*/

// This configuration is used to enable a thread to check hotplug event
// and reset USB stack to avoid memory leak, only for example.
#define CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS  1

// USB speed
#define CONFIG_CDC_ACM_SPEED					USB_SPEED_HIGH

// Echo asynchronously, for transfer size larger than packet size. While fpr
// transfer size less than packet size, the synchronous way is preferred.
#define CONFIG_USBD_CDC_ACM_ASYNC_XFER			0

// Asynchronous transfer size
#define CONFIG_CDC_ACM_ASYNC_XFER_SIZE			4096U

// Do not change the settings unless indeed necessary
#if (CONFIG_CDC_ACM_SPEED == USB_SPEED_HIGH)
#define CDC_ACM_TX_XFER_SIZE					CDC_ACM_HS_BULK_IN_PACKET_SIZE
#else
#define CDC_ACM_TX_XFER_SIZE					CDC_ACM_FS_BULK_IN_PACKET_SIZE
#endif

/* Private types -------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

static u8 cdc_acm_cb_init(void);
static u8 cdc_acm_cb_deinit(void);
static u8 cdc_acm_cb_setup(u8 cmd, u8 *pbuf, u16 length, u16 value);
static u8 cdc_acm_cb_receive(u8 *pbuf, u32 Len);

/* Private variables ---------------------------------------------------------*/

static usbd_cdc_acm_cb_t cdc_acm_cb = {
	cdc_acm_cb_init,
	cdc_acm_cb_deinit,
	cdc_acm_cb_setup,
	cdc_acm_cb_receive
};

static usbd_cdc_acm_line_coding_t cdc_acm_line_coding;

static u16 cdc_acm_ctrl_line_state;

static usbd_config_t cdc_acm_cfg = {
	.speed = CONFIG_CDC_ACM_SPEED,
	.max_ep_num = 4U,
	.rx_fifo_size = 512U,
	.nptx_fifo_size = 256U,
	.ptx_fifo_size = 64U,
	.dma_enable = FALSE,
	.self_powered = CDC_ACM_SELF_POWERED,
	.isr_priority = 4U,
};

#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
static u8 cdc_acm_async_xfer_buf[CONFIG_CDC_ACM_ASYNC_XFER_SIZE];
static u16 cdc_acm_async_xfer_buf_pos = 0;
static volatile int cdc_acm_async_xfer_busy = 0;
static _sema cdc_acm_async_xfer_sema;
#endif

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes the CDC media layer
  * @param  None
  * @retval Status
  */
static u8 cdc_acm_cb_init(void)
{
	usbd_cdc_acm_line_coding_t *lc = &cdc_acm_line_coding;

	lc->bitrate = 150000;
	lc->format = 0x00;
	lc->parity_type = 0x00;
	lc->data_type = 0x08;

#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
	cdc_acm_async_xfer_buf_pos = 0;
	cdc_acm_async_xfer_busy = 0;
#endif

	return HAL_OK;
}

/**
  * @brief  DeInitializes the CDC media layer
  * @param  None
  * @retval Status
  */
static u8 cdc_acm_cb_deinit(void)
{
#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
	cdc_acm_async_xfer_buf_pos = 0;
	cdc_acm_async_xfer_busy = 0;
#endif
	return HAL_OK;
}

/**
  * @brief  Data received over USB OUT endpoint are sent over CDC interface through this function.
  * @param  Buf: RX buffer
  * @param  Len: RX data length (in bytes)
  * @retval Status
  */
static u8 cdc_acm_cb_receive(u8 *buf, u32 len)
{
#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
	u8 ret = HAL_OK;
	if (0 == cdc_acm_async_xfer_busy) {
		if ((cdc_acm_async_xfer_buf_pos + len) > CONFIG_CDC_ACM_ASYNC_XFER_SIZE) {
			len = CONFIG_CDC_ACM_ASYNC_XFER_SIZE - cdc_acm_async_xfer_buf_pos;  // extra data discarded
		}

		rtw_memcpy((void *)((u32)cdc_acm_async_xfer_buf + cdc_acm_async_xfer_buf_pos), buf, len);
		cdc_acm_async_xfer_buf_pos += len;
		if (cdc_acm_async_xfer_buf_pos >= CONFIG_CDC_ACM_ASYNC_XFER_SIZE) {
			cdc_acm_async_xfer_buf_pos = 0;
			rtw_up_sema(&cdc_acm_async_xfer_sema);
		}
	} else {
		printf("\n[CDC] Busy, discarded %d bytes\n", len);
		ret = HAL_BUSY;
	}

	usbd_cdc_acm_receive();
	return ret;
#else
	usbd_cdc_acm_transmit(buf, len);
	return usbd_cdc_acm_receive();
#endif
}

/**
  * @brief  Handle the CDC class control requests
  * @param  cmd: Command code
  * @param  buf: Buffer containing command data (request parameters)
  * @param  len: Number of data to be sent (in bytes)
  * @retval Status
  */
static u8 cdc_acm_cb_setup(u8 cmd, u8 *pbuf, u16 len, u16 value)
{
	usbd_cdc_acm_line_coding_t *lc = &cdc_acm_line_coding;

	switch (cmd) {
	case CDC_SEND_ENCAPSULATED_COMMAND:
		/* Do nothing */
		break;

	case CDC_GET_ENCAPSULATED_RESPONSE:
		/* Do nothing */
		break;

	case CDC_SET_COMM_FEATURE:
		/* Do nothing */
		break;

	case CDC_GET_COMM_FEATURE:
		/* Do nothing */
		break;

	case CDC_CLEAR_COMM_FEATURE:
		/* Do nothing */
		break;

	case CDC_SET_LINE_CODING:
		if (len == CDC_ACM_LINE_CODING_SIZE) {
			lc->bitrate = (u32)(pbuf[0] | (pbuf[1] << 8) | (pbuf[2] << 16) | (pbuf[3] << 24));
			lc->format = pbuf[4];
			lc->parity_type = pbuf[5];
			lc->data_type = pbuf[6];
		}
		break;

	case CDC_GET_LINE_CODING:
		pbuf[0] = (u8)(lc->bitrate & 0xFF);
		pbuf[1] = (u8)((lc->bitrate >> 8) & 0xFF);
		pbuf[2] = (u8)((lc->bitrate >> 16) & 0xFF);
		pbuf[3] = (u8)((lc->bitrate >> 24) & 0xFF);
		pbuf[4] = lc->format;
		pbuf[5] = lc->parity_type;
		pbuf[6] = lc->data_type;
		break;

	case CDC_SET_CONTROL_LINE_STATE:
		/*
		wValue:	wValue, Control Signal Bitmap
				D2-15:	Reserved, 0
				D1:	RTS, 0 - Deactivate, 1 - Activate
				D0:	DTR, 0 - Not Present, 1 - Present
		*/
		if (cdc_acm_ctrl_line_state != value) {
			cdc_acm_ctrl_line_state = value;
			if (value & 0x02) {
				printf("\n[CDC] VCOM port activated/deactivated\n");
#if CONFIG_CDC_ACM_NOTIFY
				usbd_cdc_acm_notify_serial_state(CDC_ACM_CTRL_DSR | CDC_ACM_CTRL_DCD);
#endif
			}
		}

		break;

	case CDC_SEND_BREAK:
		/* Do nothing */
		break;

	default:
		break;
	}

	return HAL_OK;
}

#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
static void cdc_acm_check_usb_status_thread(void *param)
{
	int ret = 0;
	int usb_status = USBD_ATTACH_STATUS_INIT;
	static int old_usb_status = USBD_ATTACH_STATUS_INIT;

	UNUSED(param);

	for (;;) {
		rtw_mdelay_os(100);
		usb_status = usbd_get_status();
		if (old_usb_status != usb_status) {
			old_usb_status = usb_status;
			if (usb_status == USBD_ATTACH_STATUS_DETACHED) {
				printf("\n[CDC] USB DETACHED\n");
				usbd_cdc_acm_deinit();
				ret = usbd_deinit();
				if (ret != 0) {
					printf("\n[CDC] Fail to de-init USBD driver\n");
					break;
				}
				rtw_mdelay_os(100);
				printf("\n[CDC] Free heap size: 0x%lx\n", rtw_getFreeHeapSize());
				ret = usbd_init(&cdc_acm_cfg);
				if (ret != 0) {
					printf("\n[CDC] Fail to re-init USBD driver\n");
					break;
				}
				ret = usbd_cdc_acm_init(CONFIG_CDC_ACM_SPEED, &cdc_acm_cb);
				if (ret != 0) {
					printf("\n[CDC] Fail to re-init CDC ACM class\n");
					usbd_deinit();
					break;
				}
			} else if (usb_status == USBD_ATTACH_STATUS_ATTACHED) {
				printf("\n[CDC] USB ATTACHED\n");
			} else {
				printf("\n[CDC] USB INIT\n");
			}
		}
	}

	rtw_thread_exit();
}
#endif // CONFIG_USDB_MSC_CHECK_USB_STATUS

#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
static void cdc_acm_xfer_thread(void *param)
{
	u8 ret;
	u8 *xfer_buf;
	u32 xfer_len;

	for (;;) {
		if (rtw_down_sema(&cdc_acm_async_xfer_sema)) {
			xfer_len = CONFIG_CDC_ACM_ASYNC_XFER_SIZE;
			xfer_buf = cdc_acm_async_xfer_buf;
			cdc_acm_async_xfer_busy = 1;
			printf("\n[CDC] Start transfer %d bytes\n", CONFIG_CDC_ACM_ASYNC_XFER_SIZE);
			while (xfer_len > 0) {
				if (xfer_len > CDC_ACM_TX_XFER_SIZE) {
					ret = usbd_cdc_acm_transmit(xfer_buf, CDC_ACM_TX_XFER_SIZE);
					if (ret == HAL_OK) {
						xfer_len -= CDC_ACM_TX_XFER_SIZE;
						xfer_buf += CDC_ACM_TX_XFER_SIZE;
					} else { // HAL_BUSY
						printf("\n[CDC] Busy to transmit data, retry[1]\n");
						rtw_udelay_os(200);
					}
				} else {
					ret = usbd_cdc_acm_transmit(xfer_buf, xfer_len);
					if (ret == HAL_OK) {
						xfer_len = 0;
						cdc_acm_async_xfer_busy = 0;
						printf("\n[CDC] Transmit done\n");
						break;
					} else { // HAL_BUSY
						printf("\n[CDC] Busy to transmit data, retry[2]\n");
						rtw_udelay_os(200);
					}
				}
			}
		}
	}
}
#endif

static void example_usbd_cdc_acm_thread(void *param)
{
	int ret = 0;
#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
	struct task_struct check_task;
#endif
#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
	struct task_struct xfer_task;
#endif

	UNUSED(param);

#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
	rtw_init_sema(&cdc_acm_async_xfer_sema, 0);
#endif

	ret = usbd_init(&cdc_acm_cfg);
	if (ret != HAL_OK) {
		printf("\n[CDC] Fail to init USB device driver\n");
		goto exit_usbd_init_fail;
	}

	ret = usbd_cdc_acm_init(CONFIG_CDC_ACM_SPEED, &cdc_acm_cb);
	if (ret != HAL_OK) {
		printf("\n[CDC] Fail to init CDC ACM class\n");
		goto exit_usbd_cdc_acm_init_fail;
	}

#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
	ret = rtw_create_task(&check_task, "cdc_check_usb_status_thread", 512, tskIDLE_PRIORITY + 2, cdc_acm_check_usb_status_thread, NULL);
	if (ret != pdPASS) {
		printf("\n[CDC] Fail to create CDC ACM status check thread\n");
		goto exit_create_check_task_fail;
	}
#endif

#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
	// The priority of transfer thread shall be lower than USB isr priority
	ret = rtw_create_task(&xfer_task, "cdc_acm_xfer_thread", 512, tskIDLE_PRIORITY + 2, cdc_acm_xfer_thread, NULL);
	if (ret != pdPASS) {
		printf("\n[CDC] Fail to create CDC ACM transfer thread\n");
		goto exit_create_xfer_task_fail;
	}
#endif

	rtw_mdelay_os(100);

	printf("\n[CDC] CDC ACM demo started\n");

	rtw_thread_exit();

	return;

#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
exit_create_xfer_task_fail:
#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
	rtw_delete_task(&check_task);
#endif
#endif

#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
exit_create_check_task_fail:
	usbd_cdc_acm_deinit();
#endif

exit_usbd_cdc_acm_init_fail:
	usbd_deinit();

exit_usbd_init_fail:
#if CONFIG_USBD_CDC_ACM_ASYNC_XFER
	rtw_free_sema(&cdc_acm_async_xfer_sema);
#endif

	rtw_thread_exit();
}

/**
  * @brief  USB download de-initialize
  * @param  None
  * @retval Result of the operation: 0 if success else fail
  */
void example_usbd_cdc_acm(void)
{
	int ret;
	struct task_struct task;

	ret = rtw_create_task(&task, "example_usbd_cdc_acm_thread", 1024, tskIDLE_PRIORITY + 5, example_usbd_cdc_acm_thread, NULL);
	if (ret != pdPASS) {
		printf("\n[CDC] Fail to create CDC ACM thread\n");
	}
}

#endif

