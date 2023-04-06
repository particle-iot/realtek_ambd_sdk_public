/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      app_task.c
   * @brief     Routines to create App task and handle events & messages
   * @author    jane
   * @date      2017-06-02
   * @version   v1.0
   **************************************************************************************
   * @attention
   * <h2><center>&copy; COPYRIGHT 2017 Realtek Semiconductor Corporation</center></h2>
   **************************************************************************************
  */

/*============================================================================*
 *                              Header Files
 *============================================================================*/
#include <platform_opts_bt.h>
#if defined(CONFIG_BT_DISTANCE_DETECTOR) && CONFIG_BT_DISTANCE_DETECTOR
#include <stdio.h>
#include <string.h>
#include <gap.h>
#include <gap_le.h>
#include <gap_scan.h>
#include <gap_msg.h>
#include <trace_app.h>
#include <app_msg.h>
#include <basic_types.h>
#include <bt_distance_detector_app.h>
#include <bt_distance_detector_handle_message.h>
#include <bt_distance_detector_mode.h>
#include <os_msg.h>
#include <os_task.h>
#include <os_timer.h>

/** @defgroup  SCATTERNET_APP_TASK Scatternet App Task
    * @brief This file handles the implementation of application task related functions.
    *
    * Create App task and handle events & messages
    * @{
    */
/*============================================================================*
 *                              Macros
 *============================================================================*/

#define BT_DISTANCE_DETECTOR_APP_TASK_PRIORITY             1         //!< Task priorities
#define BT_DISTANCE_DETECTOR_APP_TASK_STACK_SIZE           256 * 6   //!< Task stack size
#define BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_GAP_MESSAGE     0x20      //!< GAP message queue size
#define BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_IO_MESSAGE      0x20      //!< IO message queue size
#define BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_EVENT_MESSAGE   (BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_GAP_MESSAGE + \
                                                            BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_IO_MESSAGE) //!< Event message queue size

#define BT_DISTANCE_DETECTOR_TRANSFER_TASK_PRIORITY        1         //!< Task priorities
#define BT_DISTANCE_DETECTOR_TRANSFER_TASK_STACK_SIZE      256 * 6   //!< Task stack size
/*============================================================================*
 *                              Variables
 *============================================================================*/
void *bt_distance_detector_app_task_handle;   //!< APP Task handle
void *bt_distance_detector_evt_queue_handle;  //!< Event queue handle
void *bt_distance_detector_io_queue_handle;   //!< IO queue handle

void *bt_distance_detector_transfer_task_handle; //!< Transfer Task handle
void *bt_distance_detector_transfer_queue_handle; //!< Transfer queue handle

extern T_GAP_DEV_STATE bt_distance_detector_gap_dev_state;

/*============================================================================*
 *                              Functions
 *============================================================================*/
bool bt_distance_detector_send_msg_to_apptask(uint16_t subtype)
{
    uint8_t event = EVENT_IO_TO_APP;
    T_IO_MSG io_msg;
    io_msg.type = IO_MSG_TYPE_QDECODE;
    io_msg.subtype = subtype;

    if (os_msg_send(bt_distance_detector_io_queue_handle, &io_msg, 0) == false)
    {
        APP_PRINT_INFO0("[app_task] bt_distance_detector_send_msg_to_apptask: send_io_msg_to_app fail");
        return false;
    }
    if (os_msg_send(bt_distance_detector_evt_queue_handle, &event, 0) == false)
    {
        APP_PRINT_INFO0("[app_task] bt_distance_detector_app_msg_to_apptask: send_evt_msg_to_app fail");
        return false;
    }
    return true;
}

/**
 * @brief        App task to handle events & messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
void bt_distance_detector_app_main_task(void *p_param)
{
    (void) p_param;
    uint8_t event;

    os_msg_queue_create(&bt_distance_detector_io_queue_handle, BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
    os_msg_queue_create(&bt_distance_detector_evt_queue_handle, BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

    gap_start_bt_stack(bt_distance_detector_evt_queue_handle, bt_distance_detector_io_queue_handle, BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_GAP_MESSAGE);

    bt_distance_detector_timer_init();

    bt_distance_detector_scan_result_init(BT_DISTANCE_DETECTOR_MAX_NODE_COUNT);
    bt_distance_detector_flash_init();
    bt_distance_detector_flash_restore();

    while (true)
    {
        if (os_msg_recv(bt_distance_detector_evt_queue_handle, &event, 0xFFFFFFFF) == true)
        {
            if (event == EVENT_IO_TO_APP)
            {
                T_IO_MSG io_msg;
                if (os_msg_recv(bt_distance_detector_io_queue_handle, &io_msg, 0) == true)
                {
                    bt_distance_detector_app_handle_io_msg(io_msg);
                }
            }
            else
            {
                gap_handle_msg(event);
            }
        }
    }
}

/**
 * @brief  Initialize App task
 * @return void
 */
void bt_distance_detector_app_task_init()
{
    os_task_create(&bt_distance_detector_app_task_handle, "bt_distance_detector_app", bt_distance_detector_app_main_task, 0, 
                BT_DISTANCE_DETECTOR_APP_TASK_STACK_SIZE, BT_DISTANCE_DETECTOR_APP_TASK_PRIORITY);
}

void bt_distance_detector_app_task_deinit(void)
{
	if (bt_distance_detector_app_task_handle) {
		os_task_delete(bt_distance_detector_app_task_handle);
	}
	if (bt_distance_detector_io_queue_handle) {
		os_msg_queue_delete(bt_distance_detector_io_queue_handle);
	}
	if (bt_distance_detector_evt_queue_handle) {
		os_msg_queue_delete(bt_distance_detector_evt_queue_handle);
	}

	bt_distance_detector_io_queue_handle = NULL;
	bt_distance_detector_evt_queue_handle = NULL;
	bt_distance_detector_app_task_handle = NULL;

	bt_distance_detector_scan_result_deinit();
	bt_distance_detector_timer_deinit();

	bt_distance_detector_gap_dev_state.gap_init_state = 0;
	bt_distance_detector_gap_dev_state.gap_adv_sub_state = 0;
	bt_distance_detector_gap_dev_state.gap_adv_state = 0;
	bt_distance_detector_gap_dev_state.gap_scan_state = 0;
	bt_distance_detector_gap_dev_state.gap_conn_state = 0;

}

/* ********************************************************************************************************** */
bool bt_distance_detector_send_msg_to_transfertask(device_msg_t p_msg)
{
	if (bt_distance_detector_transfer_queue_handle != NULL)
	{
		if (os_msg_send(bt_distance_detector_transfer_queue_handle, &p_msg, 0) == false)
		{
			APP_PRINT_INFO0("[transfer_task] bt_distance_detector_send_msg_to_transfertask: send msg fail");
			printf("[transfer_task] bt_distance_detector_send_msg_to_transfertask: send msg fail\r\n");
			return false;
		}
	}

	return true;
}

void bt_distance_detector_transfer_task(void *param)
{
	(void) param;
	device_msg_t dev_info;

	os_msg_queue_create(&bt_distance_detector_transfer_queue_handle, BT_DISTANCE_DETECTOR_MAX_NUMBER_OF_IO_MESSAGE, sizeof(device_msg_t));

	while(true)
	{
		if (os_msg_recv(bt_distance_detector_transfer_queue_handle, &dev_info, 0xFFFFFFFF) == true)
		{
			/*user can handle their applications in the transfer_task*/
			if (dev_info.flag == 0) //msg from app main task
			{
				bt_distance_detector_check_msg(dev_info);
			}
			else //msg from 60s-timer
			{
				bt_distance_detector_beacon_num_check();
				os_timer_start(&beacon_num_check_timer); //start next 60s-timer to check beacon num
			}
		}
	}
}

void bt_distance_detector_transfer_task_init()
{
	os_task_create(&bt_distance_detector_transfer_task_handle, "bt_distance_detector_transfer_task", bt_distance_detector_transfer_task,
					0, BT_DISTANCE_DETECTOR_TRANSFER_TASK_STACK_SIZE, BT_DISTANCE_DETECTOR_TRANSFER_TASK_PRIORITY);
}

void bt_distance_detector_transfer_task_deinit(void)
{
	if (bt_distance_detector_transfer_task_handle) {
		os_task_delete(bt_distance_detector_transfer_task_handle);
	}
	if (bt_distance_detector_transfer_queue_handle) {
		os_msg_queue_delete(bt_distance_detector_transfer_queue_handle);
	}

	bt_distance_detector_transfer_task_handle = NULL;
	bt_distance_detector_transfer_queue_handle = NULL;
}
/** @} */ /* End of group SCATTERNET_APP_TASK */
#endif