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
#if defined(CONFIG_BT_TAG_SCANNER) && CONFIG_BT_TAG_SCANNER
#include <stdio.h>
#include <os_msg.h>
#include <os_task.h>
#include <gap.h>
#include <gap_le.h>
#include <trace_app.h>
#include <app_msg.h>
#include <bt_tag_scanner_app_task.h>
#include <bt_tag_scanner_app.h>
#include <bt_tag_scanner_keep_message.h>
#include <gap_scan.h>
#include <basic_types.h>
#include <gap_msg.h>
#include <os_mem.h>

/** @defgroup  SCATTERNET_APP_TASK Scatternet App Task
    * @brief This file handles the implementation of application task related functions.
    *
    * Create App task and handle events & messages
    * @{
    */
/*============================================================================*
 *                              Macros
 *============================================================================*/

#define BT_TAG_SCANNER_APP_TASK_PRIORITY             1         //!< Task priorities
#define BT_TAG_SCANNER_APP_TASK_STACK_SIZE           256 * 6   //!< Task stack size
#define BT_TAG_SCANNER_MAX_NUMBER_OF_GAP_MESSAGE     0x20      //!< GAP message queue size
#define BT_TAG_SCANNER_MAX_NUMBER_OF_IO_MESSAGE      0x20      //!< IO message queue size
#define BT_TAG_SCANNER_MAX_NUMBER_OF_EVENT_MESSAGE   (BT_TAG_SCANNER_MAX_NUMBER_OF_GAP_MESSAGE + BT_TAG_SCANNER_MAX_NUMBER_OF_IO_MESSAGE) //!< Event message queue size

#define BT_TAG_SCANNER_TRANSFER_TASK_PRIORITY        1         //!< Task priorities
#define BT_TAG_SCANNER_TRANSFER_TASK_STACK_SIZE      256 * 6   //!< Task stack size
/*============================================================================*
 *                              Variables
 *============================================================================*/
void *bt_tag_scanner_app_task_handle;   //!< APP Task handle
void *bt_tag_scanner_evt_queue_handle;  //!< Event queue handle
void *bt_tag_scanner_io_queue_handle;   //!< IO queue handle

void *bt_tag_scanner_transfer_task_handle; //!< Transfer Task handle
void *bt_tag_scanner_transfer_queue_handle; //!< Transfer queue handle

extern T_GAP_DEV_STATE bt_tag_scanner_gap_dev_state;

/*============================================================================*
 *                              Functions
 *============================================================================*/
void bt_tag_scanner_app_main_task(void *p_param);

bool bt_tag_scanner_app_send_msg_to_apptask(T_IO_MSG *p_msg)
{
    uint8_t event = EVENT_IO_TO_APP;

    if (os_msg_send(bt_tag_scanner_io_queue_handle, &p_msg, 0) == false)
    {
        APP_PRINT_INFO0("[app_task] bt_tag_scanner_app_send_msg_to_apptask: send_io_msg_to_app fail");
        return false;
    }
    if (os_msg_send(bt_tag_scanner_evt_queue_handle, &event, 0) == false)
    {
        APP_PRINT_INFO0("[app_task] bt_tag_scanner_app_send_msg_to_apptask: send_evt_msg_to_app fail");
        return false;
    }
    return true;
}

/**
 * @brief  Initialize App task
 * @return void
 */
void bt_tag_scanner_app_task_init()
{
    os_task_create(&bt_tag_scanner_app_task_handle, "bt_tag_scanner_app", bt_tag_scanner_app_main_task, 0, 
                BT_TAG_SCANNER_APP_TASK_STACK_SIZE, BT_TAG_SCANNER_APP_TASK_PRIORITY);
}


/**
 * @brief        App task to handle events & messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
void bt_tag_scanner_app_main_task(void *p_param)
{
    (void) p_param;
    uint8_t event;

    os_msg_queue_create(&bt_tag_scanner_io_queue_handle, BT_TAG_SCANNER_MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
    os_msg_queue_create(&bt_tag_scanner_evt_queue_handle, BT_TAG_SCANNER_MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

    gap_start_bt_stack(bt_tag_scanner_evt_queue_handle, bt_tag_scanner_io_queue_handle, BT_TAG_SCANNER_MAX_NUMBER_OF_GAP_MESSAGE);

    rpl_init(BT_TAG_SCANNER_RPL_ENTRY_NUM_MAX);
    bt_tag_scanner_flash_init();
    bt_tag_scanner_flash_restore();

    while (true)
    {
        if (os_msg_recv(bt_tag_scanner_evt_queue_handle, &event, 0xFFFFFFFF) == true)
        {
            if (event == EVENT_IO_TO_APP)
            {
                T_IO_MSG io_msg;
                if (os_msg_recv(bt_tag_scanner_io_queue_handle, &io_msg, 0) == true)
                {
                    bt_tag_scanner_app_handle_io_msg(io_msg);
                }
            }
            else
            {
                gap_handle_msg(event);
            }
        }
    }
}

void bt_tag_scanner_app_task_deinit(void)
{
	if (bt_tag_scanner_app_task_handle) {
		os_task_delete(bt_tag_scanner_app_task_handle);
	}
	if (bt_tag_scanner_io_queue_handle) {
		os_msg_queue_delete(bt_tag_scanner_io_queue_handle);
	}
	if (bt_tag_scanner_evt_queue_handle) {
		os_msg_queue_delete(bt_tag_scanner_evt_queue_handle);
	}
	bt_tag_scanner_io_queue_handle = NULL;
	bt_tag_scanner_evt_queue_handle = NULL;
	bt_tag_scanner_app_task_handle = NULL;

	bt_tag_scanner_gap_dev_state.gap_init_state = 0;
	bt_tag_scanner_gap_dev_state.gap_adv_sub_state = 0;
	bt_tag_scanner_gap_dev_state.gap_adv_state = 0;
	bt_tag_scanner_gap_dev_state.gap_scan_state = 0;
	bt_tag_scanner_gap_dev_state.gap_conn_state = 0;

}

/* ********************************************************************************************************** */
void bt_tag_scanner_user_handle_tag_info(tag_info_t *tag_info)
{
	uint8_t addr[6] = {0};
	uint8_t seq[4] = {0};

	for(uint8_t i = 0; i < 6; i++)
	{
		addr[i] = tag_info->addr[i];//addr
	}
	for(uint8_t i = 0; i < 4; i++)
	{
		seq[i] = tag_info->seq[i];//seq
	}
	printf("[transfer task] addr: 0x%02x:%02x:%02x:%02x:%02x:%02x, seq: 0x%02x%02x%02x%02x\r\n", 
			addr[0], addr[1], addr[2], addr[3], addr[4], addr[5], 
			seq[0], seq[1], seq[2], seq[3]);
}

bool bt_tag_scanner_send_msg_to_transfertask(tag_info_t *p_msg)
{
	if (bt_tag_scanner_transfer_queue_handle != NULL)
	{
		if (os_msg_send(bt_tag_scanner_transfer_queue_handle, &p_msg, 0) == false)
		{
			APP_PRINT_INFO0("[transfer_task] bt_tag_scanner_send_msg_to_transfertask: send msg fail");
			printf("[transfer_task] bt_tag_scanner_send_msg_to_transfertask: send msg fail\r\n");
			return false;
		}
	}

	return true;
}

void bt_tag_scanner_transfer_task(void *param)
{
	(void) param;
	tag_info_t *msg_buffer = NULL;

	os_msg_queue_create(&bt_tag_scanner_transfer_queue_handle, BT_TAG_SCANNER_MAX_NUMBER_OF_IO_MESSAGE, sizeof(tag_info_t *));

	while(true)
	{
		if (os_msg_recv(bt_tag_scanner_transfer_queue_handle, &msg_buffer, 0xFFFFFFFF) == true)
		{
			if (msg_buffer)
			{
				/*user can handle their applications in the transfer_task*/
				bt_tag_scanner_user_handle_tag_info(msg_buffer);

				os_mem_free(msg_buffer);
				msg_buffer = NULL;
			}
		}

	}
}

void bt_tag_scanner_transfer_task_init()
{
	os_task_create(&bt_tag_scanner_transfer_task_handle, "bt_tag_scanner_transfer_task", bt_tag_scanner_transfer_task,
					0, BT_TAG_SCANNER_TRANSFER_TASK_STACK_SIZE, BT_TAG_SCANNER_TRANSFER_TASK_PRIORITY);
}

void bt_tag_scanner_transfer_task_deinit(void)
{
	if (bt_tag_scanner_transfer_task_handle) {
		os_task_delete(bt_tag_scanner_transfer_task_handle);
	}
	if (bt_tag_scanner_transfer_queue_handle) {
		os_msg_queue_delete(bt_tag_scanner_transfer_queue_handle);
	}

	rpl_deinit();

	bt_tag_scanner_transfer_task_handle = NULL;
	bt_tag_scanner_transfer_queue_handle = NULL;
}

/** @} */ /* End of group SCATTERNET_APP_TASK */
#endif