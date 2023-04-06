/**
  ******************************************************************************
  * @file    example_usbd_msc_new.c
  * @author  Realsil WLAN5 Team
  * @version V1.0.0
  * @date    2021-08-10
  * @brief   This file is example of usbd mass storage class
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
#if defined(CONFIG_EXAMPLE_USBD_MSC) && CONFIG_EXAMPLE_USBD_MSC
#include "usbd.h"
#include "usbd_msc.h"
#include "osdep_service.h"

/* Private defines -----------------------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

static usbd_config_t msc_cfg = {
	.speed = USB_SPEED_HIGH,
	.max_ep_num = 4U,
	.rx_fifo_size = 512U,
	.nptx_fifo_size = 256U,
	.ptx_fifo_size = 64U,
	.dma_enable = FALSE,
	.self_powered = USBD_MSC_SELF_POWERED,
	.isr_priority = 4U,
};

/* Private functions ---------------------------------------------------------*/
// This configuration is used to enable a thread to check hotplug event
// and reset USB stack to avoid memory leak, only for example.
#define CONFIG_USBD_MSC_CHECK_USB_STATUS   1

#if CONFIG_USBD_MSC_CHECK_USB_STATUS
static void msc_check_usb_status_thread(void *param)
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
				printf("\nUSB DETACHED\n");
				usbd_msc_deinit();
				ret = usbd_deinit();
				if (ret != 0) {
					printf("\nFail to de-init USBD driver\n");
					break;
				}
				rtw_mdelay_os(100);
				printf("\nFree heap size: 0x%lx\n", rtw_getFreeHeapSize());
				ret = usbd_init(&msc_cfg);
				if (ret != 0) {
					printf("\nFail to re-init USBD driver\n");
					break;
				}
				ret = usbd_msc_init();
				if (ret != 0) {
					printf("\nFail to re-init MSC class\n");
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
#endif // CONFIG_USBD_MSC_CHECK_USB_STATUS

static void example_usbd_msc_thread(void *param)
{
	int status = 0;
#if CONFIG_USBD_MSC_CHECK_USB_STATUS
	struct task_struct task;
#endif

	UNUSED(param);

	status = usbd_init(&msc_cfg);
	if (status != HAL_OK) {
		printf("\n\rUSB device driver init fail\n\r");
		goto example_usbd_msc_thread_fail;
	}

	status = usbd_msc_init();
	if (status != HAL_OK) {
		printf("\n\rUSB MSC init fail\n\r");
		usbd_deinit();
		goto example_usbd_msc_thread_fail;
	}

#if CONFIG_USBD_MSC_CHECK_USB_STATUS
	status = rtw_create_task(&task, "msc_check_usb_status_thread", 512, tskIDLE_PRIORITY + 2, msc_check_usb_status_thread, NULL);
	if (status != pdPASS) {
		printf("\n\rUSB create status check thread fail\n\r");
		usbd_msc_deinit();
		usbd_deinit();
		goto example_usbd_msc_thread_fail;
	}
#endif // CONFIG_USBD_MSC_CHECK_USB_STATUS

	printf("\n\rUSBD MSC demo started\n\r");

example_usbd_msc_thread_fail:
	rtw_thread_exit();
}

void example_usbd_msc(void)
{
	int ret;
	struct task_struct task;

	ret = rtw_create_task(&task, "example_usbd_msc_thread", 1024, tskIDLE_PRIORITY + 5, example_usbd_msc_thread, NULL);
	if (ret != pdPASS) {
		printf("\n\rUSBD MSC create thread fail\n\r");
	}
}

#endif

