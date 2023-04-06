#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBD_HID) && CONFIG_EXAMPLE_USBD_HID
#include <platform/platform_stdlib.h>
#include "usb.h"
#include "usbd_hid_if.h"
#include "osdep_service.h"

// This configuration is used to enable a thread to check hotplug event
// and reset USB stack to avoid memory leak, only for example.
#define CONFIG_USBD_HID_CHECK_USB_STATUS  	1

//Send mouse data through monitor.
#define SHELL_MOUSE_DATA					1

//Send mouse data table. Once connected to PC, cursor of PC will do process according to array mdata[].
#define CONSTANT_MOUSE_DATA					1

#if CONSTANT_MOUSE_DATA
struct mouse_data mdata[]={
	{0,   0,   0,  50,   0,   0},	//move the cursor 50 pixels to the right   
	{0,   0,   0,   0,  50,   0},	//move the cursor down 50 pixels
	{0,   0,   0, -50,   0,   0},	//move the cursor 50 pixels to the left
	{0,   0,   0,   0, -50,   0},	//move the cursor up 50 pixels
	{0,   0,   0,   0,   0,   5},	//scroll up for 5 units
	{0,   0,   0,   0,   0,  -5},	//scroll down for 5 units
	{0,   0,   1,   0,   0,   0},	//middle button pressed
	{0,   0,   0,   0,   0,   0},	//middle button released
	{0,   1,   0,   0,   0,   0},	//right button pressed
	{0,   0,   0,   0,   0,   0},	//right button released
	{0,   0,   0,  -5,   0,   0},	//move the cursor 5 pixels to the left
	{1,   0,   0,   0,   0,   0},	//left button pressed
	{0,   0,   0,   0,   0,   0},	//left button released
};
#endif


#if SHELL_MOUSE_DATA
u32 CmdMouseData(u16 argc, u8  *argv[])
{
	struct mouse_data data;
	
	UNUSED(argc);
	
	data.left = _strtoul((const char*)(argv[0]), (char **)NULL, 10);
	data.right = _strtoul((const char*)(argv[1]), (char **)NULL, 10);
	data.middle = _strtoul((const char*)(argv[2]), (char **)NULL, 10);
	data.x_axis = _strtoul((const char*)(argv[3]), (char **)NULL, 10);
	data.y_axis = _strtoul((const char*)(argv[4]), (char **)NULL, 10);
	data.wheel = _strtoul((const char*)(argv[5]), (char **)NULL, 10);

	usbd_hid_send_mouse(&data);

	return TRUE;
}

/*exmaple:
	left button release,
	right button release, 
	middle button release,
	x_axis: move the cursor 50 pixels to the right,
	y_axos: no movement,
	wheel: no scrolling.
CMD: mouse  0   0   0   50   0   0
*/
CMD_TABLE_DATA_SECTION
const COMMAND_TABLE   mouse_data_cmd[] = {
	{(const u8*)"mouse",		8, CmdMouseData,		NULL},
};

#endif  //SHELL_MOUSE_DATA


static void hid_mouse_init(void)
{
	printf("User callback: hid mouse init\n");
}

static void hid_mouse_deinit(void)
{
	printf("User callback: hid mouse deinit\n");
}

static void hid_transmit_complete(int status)
{
	if (status != USB_ESUCCESS) {
		printf("User callback: transmit error: %d\n", status);
	}else{
		printf("User callback: tx mouse data success\n");	
	}
}

usbd_hid_usr_cb_t hid_usr_cb = {
	.init = hid_mouse_init,
	.deinit = hid_mouse_deinit,
	.transmit_complete = hid_transmit_complete,
};


#if CONFIG_USBD_HID_CHECK_USB_STATUS
static void hid_check_usb_status_thread(void *param)
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
				printf("USB DETACHED\n");
				usbd_hid_deinit();
				usb_deinit();
				
				rtw_mdelay_os(100);
				printf("Free heap size:%x\n", rtw_getFreeHeapSize());
				
				ret = usb_init(USB_SPEED_FULL);
				if (ret != 0) {
					printf("Fail to re-init USBD driver\n");
   					break;
				}
 				ret = usbd_hid_init(&hid_usr_cb);
				if (ret != 0) {
					printf("Fail to re-init USB HID class\n");
					usb_deinit();
					break;
 				}
			} else if (usb_status == USB_STATUS_ATTACHED) {
				printf("USB ATTACHED\n");
			} else {
				printf("USB INIT\n");
			}
		}
	}
    
	rtw_thread_exit();
}
#endif // CONFIG_USBD_HID_CHECK_USB_STATUS

static void example_usbd_hid_mouse_thread(void *param)
{
	int ret = 0;
        int i = 0;
    
	UNUSED(param);

	printf("\nUSBD HID demo start\n");
    
	ret = usb_init(USB_SPEED_FULL);
	if (ret != 0) {
		printf("\nFail to init USBD controller\n");
		goto example_usbd_hid_mouse_thread_fail;
	}

	ret = usbd_hid_init(&hid_usr_cb);
	if (ret != 0) {
		printf("\nFail to init HID class\n");
		usb_deinit();
		goto example_usbd_hid_mouse_thread_fail;
	}
    
#if CONFIG_USBD_HID_CHECK_USB_STATUS
	struct task_struct task;
	ret = rtw_create_task(&task, "hid_check_usb_status_thread", 512, tskIDLE_PRIORITY + 2, hid_check_usb_status_thread, NULL);
	if (ret != pdPASS) {
		printf("\nFail to create USBD HID status check thread\n");
		usbd_hid_deinit();
		usb_deinit();
		goto example_usbd_hid_mouse_thread_fail;
	}
#endif // CONFIG_USBD_HID_CHECK_USB_STATUS

#if CONSTANT_MOUSE_DATA
	while(usbd_hid_status() == 0){
		rtw_mdelay_os(100);
	}
	for( i = 0; i < sizeof(mdata)/sizeof(struct mouse_data); i++ ){
		rtw_mdelay_os(500);
		usbd_hid_send_mouse(&mdata[i]);
	}
#endif

	rtw_mdelay_os(100);

example_usbd_hid_mouse_thread_fail:
	rtw_thread_exit();
}

void example_usbd_hid_mouse(void)
{
	int ret;
	struct task_struct task;

	ret = rtw_create_task(&task, "example_usbd_hid_mouse_thread", 1024, tskIDLE_PRIORITY + 4, example_usbd_hid_mouse_thread, NULL);
	if (ret != pdPASS) {
		printf("\nFail to create USBD hid mouse thread\n");
	}
}
#endif

