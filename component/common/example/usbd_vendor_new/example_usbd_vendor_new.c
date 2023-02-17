/**
  ******************************************************************************
  * @file    example_usbd_vendor_new.c
  * @author  Realsil WLAN5 Team
  * @version V1.0.0
  * @date    2021-08-18
  * @brief   This file is example of usbd vendor class
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
#if defined(CONFIG_EXAMPLE_USBD_VENDOR) && CONFIG_EXAMPLE_USBD_VENDOR
#include "usbd.h"
#include "usbd_vendor.h"
#include "osdep_service.h"

/* Private defines -----------------------------------------------------------*/

// This configuration is used to enable a thread to check hotplug event
// and reset USB stack to avoid memory leak, only for example.
#define CONFIG_USBD_VENDOR_CHECK_USB_STATUS  1

/* Private types -------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

/* Private variables ---------------------------------------------------------*/

static usbd_config_t vendor_cfg = {
	.speed = USB_SPEED_HIGH,
	.max_ep_num = 5U,
	.rx_fifo_size = 512U,
	.nptx_fifo_size = 256U,
	.ptx_fifo_size = 248U,
	.dma_enable = FALSE,
	.self_powered = USBD_VENDOR_SELF_POWERED,
	.isr_priority = 4U,
};

/* Private functions ---------------------------------------------------------*/

#if CONFIG_USBD_VENDOR_CHECK_USB_STATUS
static void vendor_check_usb_status_thread(void *param)
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
				usbd_vendor_deinit();
				ret = usbd_deinit();
				if (ret != 0) {
					printf("\nFail to de-init USBD driver\n");
					break;
				}
				rtw_mdelay_os(100);
				printf("\nFree heap size: 0x%lx\n", rtw_getFreeHeapSize());
				ret = usbd_init(&vendor_cfg);
				if (ret != 0) {
					printf("\nFail to re-init USBD driver\n");
					break;
				}
				ret = usbd_vendor_init();
				if (ret != 0) {
					printf("\nFail to re-init USBD VENDOR class\n");
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
#endif // CONFIG_USBD_VENDOR_CHECK_USB_STATUS

static void example_usbd_vendor_thread(void *param)
{
	int ret = 0;
#if CONFIG_USBD_VENDOR_CHECK_USB_STATUS
	struct task_struct task;
#endif

	UNUSED(param);

	ret = usbd_init(&vendor_cfg);
	if (ret != HAL_OK) {
		printf("\nFail to init USB device driver\n");
		goto example_usbd_vendor_thread_fail;
	}

	ret = usbd_vendor_init();
	if (ret != HAL_OK) {
		printf("\nFail to init USB vendor class\n");
		usbd_deinit();
		goto example_usbd_vendor_thread_fail;
	}

#if CONFIG_USBD_VENDOR_CHECK_USB_STATUS
	ret = rtw_create_task(&task, "vendor_check_usb_status_thread", 512, tskIDLE_PRIORITY + 2, vendor_check_usb_status_thread, NULL);
	if (ret != pdPASS) {
		printf("\nFail to create usbd vendor status check thread\n");
		usbd_vendor_deinit();
		usbd_deinit();
		goto example_usbd_vendor_thread_fail;
	}
#endif // CONFIG_USBD_VENDOR_CHECK_USB_STATUS

	rtw_mdelay_os(100);

	printf("\nUSB device vendor demo started...\n");

example_usbd_vendor_thread_fail:

	rtw_thread_exit();
}

void example_usbd_vendor(void)
{
	int status;
	struct task_struct task;

	status = rtw_create_task(&task, "example_usbd_vendor_thread", 1024, tskIDLE_PRIORITY + 5, example_usbd_vendor_thread, NULL);
	if (status != pdPASS) {
		printf("\nFail to create USB vendor device thread: %d\n", status);
	}
}

#endif
