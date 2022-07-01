/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      bt_breeze_app_task.c
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
#if defined(CONFIG_BT_BREEZE) && CONFIG_BT_BREEZE
#include <os_msg.h>
#include <os_task.h>
#include <gap.h>
#include <gap_le.h>
#include "bt_breeze_app_task.h"
#include <app_msg.h>
#include "bt_breeze_peripheral_app.h"
#include "platform_stdlib.h"
#include <basic_types.h>
#include <gap_msg.h>

/** @defgroup  BREEZE_APP_TASK Breeze App Task
    * @brief This file handles the implementation of application task related functions.
    *
    * Create App task and handle events & messages
    * @{
    */
/*============================================================================*
 *                              Macros
 *============================================================================*/
#define BLE_BREEZE_APP_TASK_PRIORITY             4         //!< Task priorities
#define BLE_BREEZE_APP_TASK_STACK_SIZE           256 * 8   //!<  Task stack size
#define BLE_BREEZE_CALLBACK_TASK_PRIORITY        (BLE_BREEZE_APP_TASK_PRIORITY - 1)         //!< Task priorities
#define BLE_BREEZE_CALLBACK_TASK_STACK_SIZE      256 * 8   //!<  Task stack size
#define BLE_BREEZE_MAX_NUMBER_OF_GAP_MESSAGE     0x20      //!<  GAP message queue size
#define BLE_BREEZE_MAX_NUMBER_OF_IO_MESSAGE      0x20      //!<  IO message queue size
#define BLE_BREEZE_MAX_NUMBER_OF_EVENT_MESSAGE   (BLE_BREEZE_MAX_NUMBER_OF_GAP_MESSAGE + BLE_BREEZE_MAX_NUMBER_OF_IO_MESSAGE)    //!< Event message queue size
#define BLE_BREEZE_MAX_NUMBER_OF_CALLBACK_MESSAGE      0x20      //!<  IO message queue size

/*============================================================================*
 *                              Variables
 *============================================================================*/
void *ble_breeze_app_task_handle = NULL;   //!< APP Task handle
void *ble_breeze_evt_queue_handle = NULL;  //!< Event queue handle
void *ble_breeze_io_queue_handle = NULL;   //!< IO queue handle

void *ble_breeze_callback_task_handle = NULL;   //!< Callback Task handle
void *ble_breeze_callback_queue_handle = NULL;  //!< Callback Queue handle

extern T_GAP_DEV_STATE ble_breeze_gap_dev_state;

/*============================================================================*
 *                              Functions
 *============================================================================*/

/**
 * @brief        App task to handle events & messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
void ble_breeze_app_main_task(void *p_param)
{
    (void)p_param;
    uint8_t event;
	
    os_msg_queue_create(&ble_breeze_io_queue_handle, BLE_BREEZE_MAX_NUMBER_OF_IO_MESSAGE, sizeof(T_IO_MSG));
    os_msg_queue_create(&ble_breeze_evt_queue_handle, BLE_BREEZE_MAX_NUMBER_OF_EVENT_MESSAGE, sizeof(uint8_t));

    gap_start_bt_stack(ble_breeze_evt_queue_handle, ble_breeze_io_queue_handle, BLE_BREEZE_MAX_NUMBER_OF_GAP_MESSAGE);

    while (true)
    {
        if (os_msg_recv(ble_breeze_evt_queue_handle, &event, 0xFFFFFFFF) == true)
        {
            if (event == EVENT_IO_TO_APP)
            {
                T_IO_MSG io_msg;
                if (os_msg_recv(ble_breeze_io_queue_handle, &io_msg, 0) == true)
                {
                    ble_breeze_app_handle_io_msg(io_msg);
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
 * @brief        Callback task to handle messages
 * @param[in]    p_param    Parameters sending to the task
 * @return       void
 */
extern void ble_breeze_callback_handle_msg(T_CALLBACK_MSG callback_msg);
void ble_breeze_callback_main_task(void *p_param)
{
    (void)p_param;
    T_CALLBACK_MSG callback_msg;

    os_msg_queue_create(&ble_breeze_callback_queue_handle, BLE_BREEZE_MAX_NUMBER_OF_CALLBACK_MESSAGE, sizeof(T_CALLBACK_MSG));

    while (true)
    {
        if (os_msg_recv(ble_breeze_callback_queue_handle, &callback_msg, 0xFFFFFFFF) == true)
        {
            ble_breeze_callback_handle_msg(callback_msg);
        }
    }
}

/**
 * @brief  Initialize App task
 * @return void
 */
void ble_breeze_app_task_init(void)
{
    os_task_create(&ble_breeze_app_task_handle, "breeze_app", ble_breeze_app_main_task, 0, BLE_BREEZE_APP_TASK_STACK_SIZE,
                   BLE_BREEZE_APP_TASK_PRIORITY);
    os_task_create(&ble_breeze_callback_task_handle, "breeze_callback", ble_breeze_callback_main_task, 0, BLE_BREEZE_CALLBACK_TASK_STACK_SIZE,
                   BLE_BREEZE_CALLBACK_TASK_PRIORITY);
}

void ble_breeze_app_task_deinit(void)
{
    if (ble_breeze_app_task_handle) {
        os_task_delete(ble_breeze_app_task_handle);
    }
    if (ble_breeze_callback_task_handle) {
        os_task_delete(ble_breeze_callback_task_handle);
    }
    if (ble_breeze_io_queue_handle) {
        os_msg_queue_delete(ble_breeze_io_queue_handle);
    }
    if (ble_breeze_evt_queue_handle) {
        os_msg_queue_delete(ble_breeze_evt_queue_handle);
    }
    if (ble_breeze_callback_queue_handle) {
        os_msg_queue_delete(ble_breeze_callback_queue_handle);
    }

    ble_breeze_io_queue_handle = NULL;
    ble_breeze_evt_queue_handle = NULL;
    ble_breeze_callback_queue_handle = NULL;
    ble_breeze_app_task_handle = NULL;
    ble_breeze_callback_task_handle = NULL;

    ble_breeze_gap_dev_state.gap_init_state = 0;
    ble_breeze_gap_dev_state.gap_adv_sub_state = 0;
    ble_breeze_gap_dev_state.gap_adv_state = 0;
    ble_breeze_gap_dev_state.gap_scan_state = 0;
    ble_breeze_gap_dev_state.gap_conn_state = 0;
}

/** @} */ /* End of group BREEZE_APP_TASK */
#endif

