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

#define CDC_ACM_TX_BUF_SIZE			1024U
#define CDC_ACM_RX_BUF_SIZE			1024U

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
	.speed = USBD_SPEED_HIGH,
	.max_ep_num = 4U,
	.rx_fifo_size = 512U,
	.nptx_fifo_size = 256U,
	.ptx_fifo_size = 64U,
	.intr_use_ptx_fifo = TRUE,
	.dma_enable = FALSE,
	.self_powered = CDC_ACM_SELF_POWERED,
	.isr_priority = 4U,
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initializes the CDC media layer
  * @param  None
  * @retval status
  */
static u8 cdc_acm_cb_init(void)
{
	usbd_cdc_acm_line_coding_t *lc = &cdc_acm_line_coding;

	lc->bitrate = 150000;
	lc->format = 0x00;
	lc->parity_type = 0x00;
	lc->data_type = 0x08;

	return HAL_OK;
}

/**
  * @brief  DeInitializes the CDC media layer
  * @param  None
  * @retval status
  */
static u8 cdc_acm_cb_deinit(void)
{
	/* Do nothing */
	return HAL_OK;
}

/**
  * @brief  Data received over USB OUT endpoint are sent over CDC interface through this function.
  * @param  Buf: RX buffer
  * @param  Len: RX data length (in bytes)
  * @retval status
  */
static u8 cdc_acm_cb_receive(u8 *buf, u32 len)
{
	usbd_cdc_acm_transmit(buf, len);
	return usbd_cdc_acm_receive();
}

/**
  * @brief  Handle the CDC class control requests
  * @param  cmd: Command code
  * @param  buf: Buffer containing command data (request parameters)
  * @param  len: Number of data to be sent (in bytes)
  * @retval status
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
		if ((value & 0x02) && (cdc_acm_ctrl_line_state != value)) {
			cdc_acm_ctrl_line_state = value;
			printf("\nUSBD VCOM port activated\n");
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
		usb_status = usbd_get_attach_status();
		if (old_usb_status != usb_status) {
			old_usb_status = usb_status;
			if (usb_status == USBD_ATTACH_STATUS_DETACHED) {
				printf("\nUSB DETACHED\n");
				usbd_cdc_acm_deinit();
				ret = usbd_deinit();
				if (ret != 0) {
					printf("\nFail to de-init USBD driver\n");
					break;
				}
				rtw_mdelay_os(100);
				printf("\nFree heap size: 0x%x\n", rtw_getFreeHeapSize());
				ret = usbd_init(&cdc_acm_cfg);
				if (ret != 0) {
					printf("\nFail to re-init USBD driver\n");
					break;
				}
				ret = usbd_cdc_acm_init(CDC_ACM_RX_BUF_SIZE, CDC_ACM_TX_BUF_SIZE, &cdc_acm_cb);
				if (ret != 0) {
					printf("\nFail to re-init USB CDC ACM class\n");
					usbd_deinit();
					break;
				}
			} else if (usb_status == USBD_ATTACH_STATUS_ATTACHED) {
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

	ret = usbd_init(&cdc_acm_cfg);
	if (ret != HAL_OK) {
		printf("\nFail to init USB device driver\n");
		goto example_usbd_cdc_acm_thread_fail;
	}

	ret = usbd_cdc_acm_init(CDC_ACM_RX_BUF_SIZE, CDC_ACM_TX_BUF_SIZE, &cdc_acm_cb);
	if (ret != HAL_OK) {
		printf("\nFail to init USB CDC ACM class\n");
		usbd_deinit();
		goto example_usbd_cdc_acm_thread_fail;
	}

#if CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS
	ret = rtw_create_task(&task, "cdc_check_usb_status_thread", 512, tskIDLE_PRIORITY + 2, cdc_acm_check_usb_status_thread, NULL);
	if (ret != pdPASS) {
		printf("\nFail to create USBD CDC ACM status check thread\n");
		usbd_cdc_acm_deinit();
		usbd_deinit();
		goto example_usbd_cdc_acm_thread_fail;
	}
#endif // CONFIG_USDB_CDC_ACM_CHECK_USB_STATUS

	rtw_mdelay_os(100);

	printf("\nUSBD CDC ACM demo started\n");

example_usbd_cdc_acm_thread_fail:

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
		printf("\nFail to create USBD CDC ACM thread\n");
	}
}

#endif

