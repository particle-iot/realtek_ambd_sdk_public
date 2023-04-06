/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      scatternet_app.c
   * @brief     This file handles BLE scatternet application routines.
   * @author    jane
   * @date      2017-06-06
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
#include <string.h>
#include <stdio.h>
#include <app_msg.h>
#include <trace_app.h>
#include <gap_scan.h>
#include <gap.h>
#include <gap_msg.h>
#include <gap_adv.h>
#include <bt_distance_detector_app_flags.h>
#include <bt_distance_detector_app_task.h>
#include <bt_distance_detector_mode.h>
#include <os_timer.h>

/** @defgroup  SCATTERNET_APP Scatternet Application
    * @brief This file handles BLE scatternet application routines.
    * @{
    */
/*============================================================================*
 *                              Variables
 *============================================================================*/

/** @defgroup  SCATTERNET_GAP_MSG GAP Message Handler
    * @brief Handle GAP Message
    * @{
    */
T_GAP_DEV_STATE bt_distance_detector_gap_dev_state = {0, 0, 0, 0, 0};                 /**< GAP device state */
/** @} */ /* End of group SCATTERNET_GAP_MSG GAP Message Handler */

/* filter scan info by uuid which the iBeacon adv used */
uint8_t bt_distance_detector_iBeacon_uuid128[16] ={0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; 

/*============================================================================*
 *                              Functions
 *============================================================================*/
void bt_distance_detector_app_handle_gap_msg(T_IO_MSG  *p_gap_msg);
/**
 * @brief    All the application messages are pre-handled in this function
 * @note     All the IO MSGs are sent to this function, then the event handling
 *           function shall be called according to the MSG type.
 * @param[in] io_msg  IO message data
 * @return   void
 */
void bt_distance_detector_app_handle_io_msg(T_IO_MSG io_msg)
{
    uint16_t msg_type = io_msg.type;

    switch (msg_type)
    {
    case IO_MSG_TYPE_BT_STATUS:
        {
            bt_distance_detector_app_handle_gap_msg(&io_msg);
        }
        break;
    case IO_MSG_TYPE_QDECODE:
        {
            if (io_msg.subtype == 0) {      //stop adv
                le_adv_stop();
            }
            else if (io_msg.subtype == 1) { //start adv
                le_adv_start();
            }
            else if (io_msg.subtype == 2) { //stop scan
                le_scan_stop();
            }
            else if (io_msg.subtype == 3) { //start scan
                le_scan_start();
            } 
            else if (io_msg.subtype == 4) { //clear all records
                bt_distance_detector_scan_result_clear();
            }
        }
        break;
    default:
        break;
    }
}

/**
 * @brief    Handle msg GAP_MSG_LE_DEV_STATE_CHANGE
 * @note     All the gap device state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] new_state  New gap device state
 * @param[in] cause GAP device state change cause
 * @return   void
 */
void bt_distance_detector_app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause)
{
    APP_PRINT_INFO4("bt_distance_detector_app_handle_dev_state_evt: init state  %d, adv state %d, scan state %d, cause 0x%x",
                    new_state.gap_init_state, new_state.gap_adv_state,
                    new_state.gap_scan_state, cause);

    if (bt_distance_detector_gap_dev_state.gap_init_state != new_state.gap_init_state)
    {
        if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY)
        {
            APP_PRINT_INFO0("GAP stack ready");
            printf("\r\nGAP stack ready\r\n");

            /*stack ready*/
            le_scan_start();
            le_adv_start();

            os_timer_start(&beacon_num_check_timer);
        }
    }

    if (bt_distance_detector_gap_dev_state.gap_scan_state != new_state.gap_scan_state)
    {
        if (new_state.gap_scan_state == GAP_SCAN_STATE_IDLE)
        {
            APP_PRINT_INFO0("GAP scan stopped");
            //printf("GAP scan stopped\r\n");
        }
        else if (new_state.gap_scan_state == GAP_SCAN_STATE_SCANNING)
        {
            APP_PRINT_INFO0("GAP scan start");//start scan, Check after UUID filtering, check ok and save
            //printf("GAP scan start\r\n");
        }
    }

    if (bt_distance_detector_gap_dev_state.gap_adv_state != new_state.gap_adv_state)
    {
        if (new_state.gap_adv_state == GAP_ADV_STATE_IDLE)
        {
            APP_PRINT_INFO0("GAP adv stopped");
            //printf("GAP adv stopped\n\r");
        }
        else if (new_state.gap_adv_state == GAP_ADV_STATE_ADVERTISING)
        {
            APP_PRINT_INFO0("GAP adv start");
            //printf("GAP adv start\n\r");
        }
    }

    if (bt_distance_detector_gap_dev_state.gap_scan_state != new_state.gap_scan_state || \
        bt_distance_detector_gap_dev_state.gap_adv_state != new_state.gap_adv_state)
    {
        if (new_state.gap_scan_state == GAP_ADV_STATE_IDLE && \
            new_state.gap_adv_state == GAP_SCAN_STATE_IDLE)
        {
            /*if device in low power mode, then keep the state for 1s*/
            if (device_mode.low_power_timer_flag == 1)
            {
                device_mode.low_power_timer_flag = 0;
                os_timer_start(&low_power_mode_timer);
            }
        }

        if (new_state.gap_scan_state == GAP_SCAN_STATE_SCANNING && \
            new_state.gap_adv_state == GAP_ADV_STATE_ADVERTISING)
        {
            os_timer_start(&high_datection_mode_timer);
        }
    }

    bt_distance_detector_gap_dev_state = new_state;

}

/**
 * @brief    All the BT GAP MSG are pre-handled in this function.
 * @note     Then the event handling function shall be called according to the
 *           subtype of T_IO_MSG
 * @param[in] p_gap_msg Pointer to GAP msg
 * @return   void
 */
void bt_distance_detector_app_handle_gap_msg(T_IO_MSG *p_gap_msg)
{
    T_LE_GAP_MSG gap_msg;
    memcpy(&gap_msg, &p_gap_msg->u.param, sizeof(p_gap_msg->u.param));

    APP_PRINT_TRACE1("bt_distance_detector_app_handle_gap_msg: subtype %d", p_gap_msg->subtype);
    switch (p_gap_msg->subtype)
    {
    case GAP_MSG_LE_DEV_STATE_CHANGE:
        {
            bt_distance_detector_app_handle_dev_state_evt(gap_msg.msg_data.gap_dev_state_change.new_state,
                                     gap_msg.msg_data.gap_dev_state_change.cause);
        }
        break;

    default:
        APP_PRINT_ERROR1("bt_distance_detector_app_handle_gap_msg: unknown subtype %d", p_gap_msg->subtype);
        break;
    }
}

int bt_distance_detector_filter_iBeacon(uint8_t *iBeacon_uuid128, T_LE_SCAN_INFO *scan_info)
{
	uint8_t buffer[32];
	uint8_t pos = 0;
	int ret = 0;

	while (pos < scan_info->data_len)
	{
		/* Length of the AD structure. */
		uint8_t length = scan_info->data[pos++];
		uint8_t type;

		if ((length > 0x01) && ((pos + length) <= 31))
		{
			/* Copy the AD Data to buffer. */
			memcpy(buffer, scan_info->data + pos + 1, length - 1);

			/* AD Type, one octet. */
			type = scan_info->data[pos];

			switch (type)
			{
			case GAP_ADTYPE_MANUFACTURER_SPECIFIC:
				{
					uint8_t buffer_len = length - 1; //the AD Data length
					uint8_t header[4] = {0x4C, 0x00, 0x02, 0x15}; //the iBeacon AD Data header 4 bytes
					device_msg_t dev_info;

					/*filter a target device */
					if (memcmp(header, buffer, 4) == 0)
					{
						if (memcmp(buffer + 4, iBeacon_uuid128, 16) == 0)
						{
							device_mode.beacon_count++;
							device_mode.two_second.current++;

							memcpy(dev_info.dev_addr, scan_info->bd_addr, 6);
							dev_info.dev_rssi = scan_info->rssi;
							dev_info.txpower = buffer[buffer_len - 1];
							dev_info.flag = 0;

							/* send msg to another task*/
							bt_distance_detector_send_msg_to_transfertask(dev_info);
						}
					}
				}
				break;

			default:
				break;
			}
		}

		pos += length;
	}
	return ret;
}

/** @defgroup  SCATTERNET_GAP_CALLBACK GAP Callback Event Handler
    * @brief Handle GAP callback event
    * @{
    */
/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT bt_distance_detector_app_gap_callback(uint8_t cb_type, void *p_cb_data)
{
    T_APP_RESULT result = APP_RESULT_SUCCESS;
    T_LE_CB_DATA *p_data = (T_LE_CB_DATA *)p_cb_data;

    switch (cb_type)
    {
    /* central reference msg*/
    case GAP_MSG_LE_SCAN_INFO:
        APP_PRINT_INFO5("GAP_MSG_LE_SCAN_INFO:adv_type 0x%x, bd_addr %s, remote_addr_type %d, rssi %d, data_len %d",
                        p_data->p_le_scan_info->adv_type,
                        TRACE_BDADDR(p_data->p_le_scan_info->bd_addr),
                        p_data->p_le_scan_info->remote_addr_type,
                        p_data->p_le_scan_info->rssi,
                        p_data->p_le_scan_info->data_len);

        /* User can split interested information by using the function as follow. */
        bt_distance_detector_filter_iBeacon(bt_distance_detector_iBeacon_uuid128, p_data->p_le_scan_info);
        break;

    case GAP_MSG_LE_READ_RSSI:
        APP_PRINT_INFO3("GAP_MSG_LE_READ_RSSI:conn_id 0x%x cause 0x%x rssi %d",
                        p_data->p_le_read_rssi_rsp->conn_id,
                        p_data->p_le_read_rssi_rsp->cause,
                        p_data->p_le_read_rssi_rsp->rssi);
        break;

    case GAP_MSG_LE_MODIFY_WHITE_LIST:
        APP_PRINT_INFO2("GAP_MSG_LE_MODIFY_WHITE_LIST: operation %d, cause 0x%x",
                        p_data->p_le_modify_white_list_rsp->operation,
                        p_data->p_le_modify_white_list_rsp->cause);
        break;

    case GAP_MSG_LE_SET_HOST_CHANN_CLASSIF:
        APP_PRINT_INFO1("GAP_MSG_LE_SET_HOST_CHANN_CLASSIF: cause 0x%x",
                        p_data->p_le_set_host_chann_classif_rsp->cause);
        break;

    default:
        APP_PRINT_ERROR1("app_gap_callback: unhandled cb_type 0x%x", cb_type);
        break;
    }
    return result;
}
/** @} */ /* End of group SCATTERNET_GAP_CALLBACK */

/** @} */ /* End of group SCATTERNET_APP */
#endif
