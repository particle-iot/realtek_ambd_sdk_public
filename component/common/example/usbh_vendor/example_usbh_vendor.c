#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBH_VENDOR) && CONFIG_EXAMPLE_USBH_VENDOR
#include <platform/platform_stdlib.h>
#include "osdep_service.h"
#include "usb.h"
#include "usbh_vendor_if.h"
#include "dwc_otg_driver.h"

_sema VendorSema;
#define CONFIG_USBH_VENDOR_MEMORY_LEAK_CHECK 1
#define CONFIG_USB_PHY_REG_RW 0

#if CONFIG_USBH_VENDOR_MEMORY_LEAK_CHECK
static void vendor_detach(void)
{
	rtw_up_sema(&VendorSema);
}
#endif

usbh_vendor_usr_cb_t vendor_usr_cb = {
	//.attach = vendor_attach,
#if CONFIG_USBH_VENDOR_MEMORY_LEAK_CHECK
	.detach = vendor_detach,
#endif
};

#if CONFIG_USBH_VENDOR_MEMORY_LEAK_CHECK
static void vendor_check_memory_leak_thread(void *param)
{
	int ret = 0;

	UNUSED(param);

	for (;;) {
		rtw_down_sema(&VendorSema);
		rtw_mdelay_os(100);//make sure disconnect handle finish before deinit.
		usbh_vendor_deinit();
		usb_deinit();
		rtw_mdelay_os(10);
		printf("deinit:%x\n", xPortGetFreeHeapSize());
		
		ret = usb_init();
		if (ret != USB_INIT_OK) {
			printf("\nFail to init USB host controller: %d\n", ret);
			break;
		}

		ret = usbh_vendor_init(&vendor_usr_cb);
		if (ret < 0) {
			printf("\nFail to init USB host vendor driver: %d\n", ret);
			usb_deinit();
			break;
		}
	}

	rtw_thread_exit();
}
#endif

void example_usbh_vendor_thread(void *param)
{
    int status;
    struct task_struct task;

    UNUSED(param);
    rtw_init_sema(&VendorSema, 0);
    status = usb_init();
    if (status != USB_INIT_OK) {
        printf("\nFail to init USB host controller: %d\n", status);
        goto error_exit;
    }

    status = usbh_vendor_init(&vendor_usr_cb);
    if (status < 0) {
        printf("\nFail to init USB host vendor driver: %d\n", status);
        usb_deinit();
	goto error_exit;
    }
	
#if CONFIG_USBH_VENDOR_MEMORY_LEAK_CHECK
    status = rtw_create_task(&task, "vendor_check_memory_leak_thread", 512, tskIDLE_PRIORITY + 2, vendor_check_memory_leak_thread, NULL);
    if (status != pdPASS) {
        printf("\nFail to create USBH vendor memory leak check thread\n");
        usbh_vendor_deinit();
        usb_deinit();
        goto error_exit;
    }
#endif

    goto example_exit;
	
error_exit:
    rtw_free_sema(&VendorSema);
example_exit:	
    rtw_thread_exit();
}


void example_usbh_vendor(void)
{
    int status;
    struct task_struct task;
    
    printf("\nUSB host vendor demo started...\n");
    status = rtw_create_task(&task, "example_usbh_vendor_thread", 512, tskIDLE_PRIORITY + 2, example_usbh_vendor_thread, NULL);
    if (status != pdPASS) {
        printf("\nFail to create USB host vendor thread: %d\n", status);
    }
}

#if CONFIG_USB_PHY_REG_RW
u32 CmdUsbDwTest(u16 argc, u8  *argv[])
{
	u32 addr;
	u8 val;
	int ret = 0;

	addr = _strtoul((const char*)(argv[0]), (char **)NULL, 16);
	ret = dwc_otg_read_phy_reg((addr & 0xFF), &val);
	if (ret < 0) {
		printf("PHY Read fail\n");
	} else {
		printf("PHY Read: %x: %x\n", (addr & 0xFF), val);
	}
	return ret;
}

u32 CmdUsbEwTest(u16 argc, u8  *argv[])
{
	u32 addr;
	u32 val;
	int ret = 0;

	addr = _strtoul((const char*)(argv[0]), (char **)NULL, 16);
	val= _strtoul((const char*)(argv[1]), (char **)NULL, 16);
	printf("PHY write: %x: %x\n", (addr & 0xFF), (val & 0xFF));
	ret = dwc_otg_write_phy_reg((addr & 0xFF), (val & 0xFF));
	if (ret < 0) {
		printf("PHY Write fail\n");
	}
	return ret;
}

CMD_TABLE_DATA_SECTION
const COMMAND_TABLE   phy_test_cmd_table[] = {
	{	(const u8*)"PHYDW",  4, CmdUsbDwTest,      (const u8*)"\tUsb phy read \n"
		"\t\t phy register read test, input phy address \n"
	},
	{	(const u8*)"PHYEW",  4, CmdUsbEwTest,      (const u8*)"\tUsb phy write \n"
		"\t\t phy register write test, input phy address and value \n"
	},
};
#endif
#endif
