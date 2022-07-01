/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      bt_breeze_peripheral_app.c
   * @brief     This file handles BLE peripheral application routines.
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
#if defined(CONFIG_BT_BREEZE) && CONFIG_BT_BREEZE
#include "platform_stdlib.h"
#include <trace_app.h>
#include <string.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_bond_le.h>
#include <profile_server.h>
#include <gap_msg.h>
#include <app_msg.h>
#include "bt_breeze_peripheral_app.h"
#include "bt_breeze_app_flags.h"
#include <gap_conn_le.h>
#include "platform_stdlib.h"

#include "ble_service.h"
#include "breeze_hal_ble.h"
#include "breeze.h"
#include "os_msg.h"
#include "osdep_service.h"

/** @defgroup  BREEZE_APP Peripheral Application
    * @brief This file handles BLE peripheral application routines.
    * @{
	*/


/*============================================================================*
 *                              Variables
 *============================================================================*/
 
T_SERVER_ID breeze_srv_id;  /**< Breeze service id */

T_GAP_DEV_STATE ble_breeze_gap_dev_state = {0, 0, 0, 0, 0};                 /**< GAP device state */
T_GAP_CONN_STATE ble_breeze_gap_conn_state = GAP_CONN_STATE_DISCONNECTED; /**< GAP connection state */ 
extern ind_cb g_indicate_cb;
extern ais_service_cb_t *ais_service_cb_info;
extern void *ble_breeze_callback_queue_handle;
/*============================================================================*
 *                              Functions
 *============================================================================*/
/** @defgroup  BREEZE_GAP_MSG GAP Message Handler
    * @brief Handle GAP Message
    * @{
    */
void ble_breeze_app_handle_gap_msg(T_IO_MSG  *p_gap_msg);
/**
 * @brief    All the application messages are pre-handled in this function
 * @note     All the IO MSGs are sent to this function, then the event handling
 *           function shall be called according to the MSG type.
 * @param[in] io_msg  IO message data
 * @return   void
 */
#if BREEZE_TEMP_WORK_AROUND
extern void ble_breeze_free_ind_notify_param(ind_notify_param_list_t *p_ind_notify_param_list);
#endif
int ble_breeze_app_handle_upstream_msg(uint16_t subtype, void *pdata)
{
	int ret = 0;
#if BREEZE_TEMP_WORK_AROUND
	ind_notify_param_t data;
	if (pdata != NULL)
		data = ((ind_notify_param_list_t *)pdata)->ind_notify_param;
#else
	ind_notify_param_t *data;
	if (pdata != NULL)
		data = pdata;
#endif

	switch (subtype) {
		case BT_BREEZE_MSG_START_ADV:
			ret = le_adv_start();
			break;
		case BT_BREEZE_MSG_STOP_ADV:
			ret = le_adv_stop();
			break;
		case BT_BREEZE_MSG_DISCONNECT:
			ret = le_disconnect(0);
			break;
		case BT_BREEZE_MSG_NOTIFY:
#if BREEZE_TEMP_WORK_AROUND
			ret = breeze_send_notify(0, breeze_srv_id, data.p_data, data.length);
			if (pdata != NULL)
				ble_breeze_free_ind_notify_param((ind_notify_param_list_t *)pdata);
#else
			ret = breeze_send_notify(0, breeze_srv_id, data->p_data, data->length);
			if (pdata != NULL)
				rtw_free(pdata);
#endif
			break;
		case BT_BREEZE_MSG_INDICATE:
#if BREEZE_TEMP_WORK_AROUND
			ret = breeze_send_indicate(0, breeze_srv_id, data.ic.p_data, data.length, data.ic.cb);
			if (pdata != NULL)
				ble_breeze_free_ind_notify_param((ind_notify_param_list_t *)pdata);
#else
			ret = breeze_send_indicate(0, breeze_srv_id, data->ic.p_data, data->length, data->ic.cb);
			if (pdata != NULL)
				rtw_free(pdata);
#endif
			break;
		default:
			break;
	}

	return ret;
}

extern int ble_breeze_app_handle_at_cmd(uint16_t subtype, void *arg);

void ble_breeze_app_handle_io_msg(T_IO_MSG io_msg)
{
    uint16_t msg_type = io_msg.type;
	uint16_t subtype;
	void *arg = NULL;
	
    switch (msg_type)
    {
    case IO_MSG_TYPE_BT_STATUS:
        {
            ble_breeze_app_handle_gap_msg(&io_msg);
        }
        break;
    case IO_MSG_TYPE_QDECODE:
        {
			subtype = io_msg.subtype;
			arg = io_msg.u.buf;
			ble_breeze_app_handle_upstream_msg(subtype, arg);
        }
        break;
	default:
        break;
    }
}

void ble_breeze_callback_handle_msg(T_CALLBACK_MSG callback_msg)
{
	BT_BREEZE_MSG_TYPE msg_type = callback_msg.type;
	uint32_t msg_data = callback_msg.data;
	void *msg_buf = callback_msg.buf;

	switch (msg_type)
	{
		case BT_BREEZE_CALLBACK_MSG_WRITE_WC:
			ais_service_cb_info->write_ais_wc(msg_buf, msg_data);
			break;
		case BT_BREEZE_CALLBACK_MSG_WRITE_WWNRC:
			ais_service_cb_info->write_ais_wwnrc(msg_buf, msg_data);
			break;
		case BT_BREEZE_CALLBACK_MSG_IC_CCC_CFG_CHANGED:
			ais_service_cb_info->ais_ic_ccc_cfg_changed(msg_data);
			break;
		case BT_BREEZE_CALLBACK_MSG_NC_CCC_CFG_CHANGED:
			ais_service_cb_info->ais_nc_ccc_cfg_changed(msg_data);
			break;
		case BT_BREEZE_CALLBACK_MSG_CONNECTED:
			ais_service_cb_info->connected();
			break;
		case BT_BREEZE_CALLBACK_MSG_DISCONNECTED:
			ais_service_cb_info->disconnected();
			break;
		case BT_BREEZE_CALLBACK_MSG_INDICATE_DONE:
			if (g_indicate_cb != NULL) {
				g_indicate_cb(0);
			}
			break;
		default:
			printf("ble_breeze_callback_handle_msg fail: callback_msg.type = 0x%x\r\n", msg_type);
			break;
	}
}

void ble_breeze_callback_send_msg(BT_BREEZE_MSG_TYPE msg_type, uint32_t msg_data, void *msg_buf)
{
	T_CALLBACK_MSG callback_msg;

	callback_msg.type = (uint16_t)msg_type;
	callback_msg.data = msg_data;
	callback_msg.buf = msg_buf;

	if (ble_breeze_callback_queue_handle != NULL) {
		if (os_msg_send(ble_breeze_callback_queue_handle, &callback_msg, 0) == false) {
			printf("ble_breeze_callback_send_msg fail: callback_msg.type = 0x%x\r\n", callback_msg.type);
		}
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
void ble_breeze_app_handle_dev_state_evt(T_GAP_DEV_STATE new_state, uint16_t cause)
{
    APP_PRINT_INFO3("ble_breeze_app_handle_dev_state_evt: init state %d, adv state %d, cause 0x%x",
                    new_state.gap_init_state, new_state.gap_adv_state, cause);
    if (ble_breeze_gap_dev_state.gap_init_state != new_state.gap_init_state)
    {
        if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY)
        {
            APP_PRINT_INFO0("GAP stack ready");
            printf("\n\r[BT Breeze] GAP stack ready\n\r");
        }
    }

    if (ble_breeze_gap_dev_state.gap_adv_state != new_state.gap_adv_state)
    {
        if (new_state.gap_adv_state == GAP_ADV_STATE_IDLE)
        {
            if (new_state.gap_adv_sub_state == GAP_ADV_TO_IDLE_CAUSE_CONN)
            {
                APP_PRINT_INFO0("GAP adv stoped: because connection created");
                printf("\n\rGAP adv stoped: because connection created\n\r");
            }
            else
            {
                APP_PRINT_INFO0("GAP adv stoped");
                printf("\n\rGAP adv stopped\n\r");
            }
        }
        else if (new_state.gap_adv_state == GAP_ADV_STATE_ADVERTISING)
        {
            APP_PRINT_INFO0("GAP adv start");
            printf("\n\rGAP adv start\n\r");
        }
    }

    ble_breeze_gap_dev_state = new_state;
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_STATE_CHANGE
 * @note     All the gap conn state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] conn_id Connection ID
 * @param[in] new_state  New gap connection state
 * @param[in] disc_cause Use this cause when new_state is GAP_CONN_STATE_DISCONNECTED
 * @return   void
 */
 
void ble_breeze_app_handle_conn_state_evt(uint8_t conn_id, T_GAP_CONN_STATE new_state, uint16_t disc_cause)
{
    APP_PRINT_INFO4("ble_breeze_app_handle_conn_state_evt: conn_id %d old_state %d new_state %d, disc_cause 0x%x",
                    conn_id, ble_breeze_gap_conn_state, new_state, disc_cause);
    switch (new_state)
    {
    case GAP_CONN_STATE_DISCONNECTED:
        {
			if ((disc_cause != (HCI_ERR | HCI_ERR_REMOTE_USER_TERMINATE))
                && (disc_cause != (HCI_ERR | HCI_ERR_LOCAL_HOST_TERMINATE)))
            {
                APP_PRINT_ERROR1("ble_breeze_app_handle_conn_state_evt: connection lost cause 0x%x", disc_cause);
            }
            printf("[BLE Breeze] BLE Disconnected, disc_cause = 0x%x\n\r", disc_cause);
            //ais_service_cb_info->disconnected();
            ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_DISCONNECTED, 0, NULL);
        }
        break;

    case GAP_CONN_STATE_CONNECTED:
        {
            uint16_t conn_interval;
            uint16_t conn_latency;
            uint16_t conn_supervision_timeout;
            uint8_t  remote_bd[6];
            T_GAP_REMOTE_ADDR_TYPE remote_bd_type;

            le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
            le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_latency, conn_id);
            le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);
            le_get_conn_addr(conn_id, remote_bd, (void *)&remote_bd_type);
            APP_PRINT_INFO5("GAP_CONN_STATE_CONNECTED:remote_bd %s, remote_addr_type %d, conn_interval 0x%x, conn_latency 0x%x, conn_supervision_timeout 0x%x",
                            TRACE_BDADDR(remote_bd), remote_bd_type,
                            conn_interval, conn_latency, conn_supervision_timeout);
            printf("[BLE Breeze] BLE Connected\n\r");
            //ais_service_cb_info->connected();
            ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_CONNECTED, 0, NULL);
        }
        break;

    default:
        break;
    }
    ble_breeze_gap_conn_state = new_state;
}

/**
 * @brief    Handle msg GAP_MSG_LE_AUTHEN_STATE_CHANGE
 * @note     All the gap authentication state events are pre-handled in this function.
 *           Then the event handling function shall be called according to the new_state
 * @param[in] conn_id Connection ID
 * @param[in] new_state  New authentication state
 * @param[in] cause Use this cause when new_state is GAP_AUTHEN_STATE_COMPLETE
 * @return   void
 */
void ble_breeze_app_handle_authen_state_evt(uint8_t conn_id, uint8_t new_state, uint16_t cause)
{
    APP_PRINT_INFO2("ble_breeze_app_handle_authen_state_evt:conn_id %d, cause 0x%x", conn_id, cause);

    switch (new_state)
    {
    case GAP_AUTHEN_STATE_STARTED:
        {
            APP_PRINT_INFO0("ble_breeze_app_handle_authen_state_evt: GAP_AUTHEN_STATE_STARTED");
        }
        break;

    case GAP_AUTHEN_STATE_COMPLETE:
        {
            if (cause == GAP_SUCCESS)
            {
                printf("Pair success\r\n");
                APP_PRINT_INFO0("ble_breeze_app_handle_authen_state_evt: GAP_AUTHEN_STATE_COMPLETE pair success");

            }
            else
            {
                printf("Pair failed: cause 0x%x\r\n", cause);
                APP_PRINT_INFO0("ble_breeze_app_handle_authen_state_evt: GAP_AUTHEN_STATE_COMPLETE pair failed");
            }
        }
        break;

    default:
        {
            APP_PRINT_ERROR1("ble_breeze_app_handle_authen_state_evt: unknown newstate %d", new_state);
        }
        break;
    }
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_MTU_INFO
 * @note     This msg is used to inform APP that exchange mtu procedure is completed.
 * @param[in] conn_id Connection ID
 * @param[in] mtu_size  New mtu size
 * @return   void
 */
void ble_breeze_app_handle_conn_mtu_info_evt(uint8_t conn_id, uint16_t mtu_size)
{
    APP_PRINT_INFO2("ble_breeze_app_handle_conn_mtu_info_evt: conn_id %d, mtu_size %d", conn_id, mtu_size);
}

/**
 * @brief    Handle msg GAP_MSG_LE_CONN_PARAM_UPDATE
 * @note     All the connection parameter update change  events are pre-handled in this function.
 * @param[in] conn_id Connection ID
 * @param[in] status  New update state
 * @param[in] cause Use this cause when status is GAP_CONN_PARAM_UPDATE_STATUS_FAIL
 * @return   void
 */
void ble_breeze_app_handle_conn_param_update_evt(uint8_t conn_id, uint8_t status, uint16_t cause)
{
    switch (status)
    {
    case GAP_CONN_PARAM_UPDATE_STATUS_SUCCESS:
        {
            uint16_t conn_interval;
            uint16_t conn_slave_latency;
            uint16_t conn_supervision_timeout;

            le_get_conn_param(GAP_PARAM_CONN_INTERVAL, &conn_interval, conn_id);
            le_get_conn_param(GAP_PARAM_CONN_LATENCY, &conn_slave_latency, conn_id);
            le_get_conn_param(GAP_PARAM_CONN_TIMEOUT, &conn_supervision_timeout, conn_id);
            APP_PRINT_INFO3("ble_breeze_app_handle_conn_param_update_evt update success:conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x",
                            conn_interval, conn_slave_latency, conn_supervision_timeout);
            //printf("ble_breeze_app_handle_conn_param_update_evt update success:conn_interval 0x%x, conn_slave_latency 0x%x, conn_supervision_timeout 0x%x\r\n",
            //                conn_interval, conn_slave_latency, conn_supervision_timeout);
        }
        break;

    case GAP_CONN_PARAM_UPDATE_STATUS_FAIL:
        {
            APP_PRINT_ERROR1("ble_breeze_app_handle_conn_param_update_evt update failed: cause 0x%x", cause);
            printf("ble_breeze_app_handle_conn_param_update_evt update failed: cause 0x%x\r\n", cause);
        }
        break;

    case GAP_CONN_PARAM_UPDATE_STATUS_PENDING:
        {
            APP_PRINT_INFO0("ble_breeze_app_handle_conn_param_update_evt update pending.");
            printf("ble_breeze_app_handle_conn_param_update_evt update pending.\r\n");
        }
        break;

    default:
        break;
    }
}

/**
 * @brief    All the BT GAP MSG are pre-handled in this function.
 * @note     Then the event handling function shall be called according to the
 *           subtype of T_IO_MSG
 * @param[in] p_gap_msg Pointer to GAP msg
 * @return   void
 */
void ble_breeze_app_handle_gap_msg(T_IO_MSG *p_gap_msg)
{
    T_LE_GAP_MSG gap_msg;
    uint8_t conn_id;
    memcpy(&gap_msg, &p_gap_msg->u.param, sizeof(p_gap_msg->u.param));

    APP_PRINT_TRACE1("ble_breeze_app_handle_gap_msg: subtype %d", p_gap_msg->subtype);
    switch (p_gap_msg->subtype)
    {
    case GAP_MSG_LE_DEV_STATE_CHANGE:
        {
            ble_breeze_app_handle_dev_state_evt(gap_msg.msg_data.gap_dev_state_change.new_state,
                                     gap_msg.msg_data.gap_dev_state_change.cause);
        }
        break;

    case GAP_MSG_LE_CONN_STATE_CHANGE:
        {
            ble_breeze_app_handle_conn_state_evt(gap_msg.msg_data.gap_conn_state_change.conn_id,
                                      (T_GAP_CONN_STATE)gap_msg.msg_data.gap_conn_state_change.new_state,
                                      gap_msg.msg_data.gap_conn_state_change.disc_cause);
        }
        break;

    case GAP_MSG_LE_CONN_MTU_INFO:
        {
            ble_breeze_app_handle_conn_mtu_info_evt(gap_msg.msg_data.gap_conn_mtu_info.conn_id,
                                         gap_msg.msg_data.gap_conn_mtu_info.mtu_size);
        }
        break;

    case GAP_MSG_LE_CONN_PARAM_UPDATE:
        {
            ble_breeze_app_handle_conn_param_update_evt(gap_msg.msg_data.gap_conn_param_update.conn_id,
                                             gap_msg.msg_data.gap_conn_param_update.status,
                                             gap_msg.msg_data.gap_conn_param_update.cause);
        }
        break;

    case GAP_MSG_LE_AUTHEN_STATE_CHANGE:
        {
            ble_breeze_app_handle_authen_state_evt(gap_msg.msg_data.gap_authen_state.conn_id,
                                        gap_msg.msg_data.gap_authen_state.new_state,
                                        gap_msg.msg_data.gap_authen_state.status);
        }
        break;

    case GAP_MSG_LE_BOND_JUST_WORK:
        {
            conn_id = gap_msg.msg_data.gap_bond_just_work_conf.conn_id;
            le_bond_just_work_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
            APP_PRINT_INFO0("GAP_MSG_LE_BOND_JUST_WORK");
        }
        break;

    case GAP_MSG_LE_BOND_PASSKEY_DISPLAY:
        {
            uint32_t display_value = 0;
            conn_id = gap_msg.msg_data.gap_bond_passkey_display.conn_id;
            le_bond_get_display_key(conn_id, &display_value);
            APP_PRINT_INFO1("GAP_MSG_LE_BOND_PASSKEY_DISPLAY:passkey %d", display_value);
            le_bond_passkey_display_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
            printf("GAP_MSG_LE_BOND_PASSKEY_DISPLAY:passkey %d", display_value);
        }
        break;

    case GAP_MSG_LE_BOND_USER_CONFIRMATION:
        {
            uint32_t display_value = 0;
            conn_id = gap_msg.msg_data.gap_bond_user_conf.conn_id;
            le_bond_get_display_key(conn_id, &display_value);
            APP_PRINT_INFO1("GAP_MSG_LE_BOND_USER_CONFIRMATION: passkey %d", display_value);
            printf("GAP_MSG_LE_BOND_USER_CONFIRMATION: passkey %d", display_value);
            //le_bond_user_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
        }
        break;

    case GAP_MSG_LE_BOND_PASSKEY_INPUT:
        {
            //uint32_t passkey = 888888;
            conn_id = gap_msg.msg_data.gap_bond_passkey_input.conn_id;
            APP_PRINT_INFO1("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d", conn_id);
            //le_bond_passkey_input_confirm(conn_id, passkey, GAP_CFM_CAUSE_ACCEPT);
            printf("GAP_MSG_LE_BOND_PASSKEY_INPUT: conn_id %d", conn_id);
        }
        break;
#if F_BT_LE_SMP_OOB_SUPPORT
    case GAP_MSG_LE_BOND_OOB_INPUT:
        {
            uint8_t oob_data[GAP_OOB_LEN] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
            conn_id = gap_msg.msg_data.gap_bond_oob_input.conn_id;
            APP_PRINT_INFO0("GAP_MSG_LE_BOND_OOB_INPUT");
            le_bond_set_param(GAP_PARAM_BOND_OOB_DATA, GAP_OOB_LEN, oob_data);
            le_bond_oob_input_confirm(conn_id, GAP_CFM_CAUSE_ACCEPT);
        }
        break;
#endif
    default:
        APP_PRINT_ERROR1("ble_breeze_app_handle_gap_msg: unknown subtype %d", p_gap_msg->subtype);
        break;
    }
}
/** @} */ /* End of group BREEZE_GAP_MSG */

/** @defgroup  BREEZE_GAP_CALLBACK GAP Callback Event Handler
    * @brief Handle GAP callback event
    * @{
    */
/**
  * @brief Callback for gap le to notify app
  * @param[in] cb_type callback msy type @ref GAP_LE_MSG_Types.
  * @param[in] p_cb_data point to callback data @ref T_LE_CB_DATA.
  * @retval result @ref T_APP_RESULT
  */
T_APP_RESULT ble_breeze_app_gap_callback(uint8_t cb_type, void *p_cb_data)
{
    T_APP_RESULT result = APP_RESULT_SUCCESS;
    T_LE_CB_DATA *p_data = (T_LE_CB_DATA *)p_cb_data;

    switch (cb_type)
    {
#if F_BT_LE_4_2_DATA_LEN_EXT_SUPPORT
    case GAP_MSG_LE_DATA_LEN_CHANGE_INFO:
        APP_PRINT_INFO3("GAP_MSG_LE_DATA_LEN_CHANGE_INFO: conn_id %d, tx octets 0x%x, max_tx_time 0x%x",
                        p_data->p_le_data_len_change_info->conn_id,
                        p_data->p_le_data_len_change_info->max_tx_octets,
                        p_data->p_le_data_len_change_info->max_tx_time);
        break;
#endif
    case GAP_MSG_LE_MODIFY_WHITE_LIST:
        APP_PRINT_INFO2("GAP_MSG_LE_MODIFY_WHITE_LIST: operation %d, cause 0x%x",
                        p_data->p_le_modify_white_list_rsp->operation,
                        p_data->p_le_modify_white_list_rsp->cause);
        break;

    default:
        APP_PRINT_ERROR1("ble_breeze_app_gap_callback: unhandled cb_type 0x%x", cb_type);
        break;
    }
    return result;
}
/** @} */ /* End of group BREEZE_GAP_CALLBACK */

/** @defgroup  BREEZE_SEVER_CALLBACK Profile Server Callback Event Handler
    * @brief Handle profile server callback event
    * @{
    */
/**
    * @brief    All the BT Profile service callback events are handled in this function
    * @note     Then the event handling function shall be called according to the
    *           service_id
    * @param    service_id  Profile service ID
    * @param    p_data      Pointer to callback data
    * @return   T_APP_RESULT, which indicates the function call is successful or not
    * @retval   APP_RESULT_SUCCESS  Function run successfully
    * @retval   others              Function run failed, and return number indicates the reason
    */ 

T_APP_RESULT ble_breeze_app_profile_callback(T_SERVER_ID service_id, void *p_data)
{
    T_APP_RESULT app_result = APP_RESULT_SUCCESS;
    if (service_id == SERVICE_PROFILE_GENERAL_ID)
    {
        T_SERVER_APP_CB_DATA *p_param = (T_SERVER_APP_CB_DATA *)p_data;
        switch (p_param->eventId)
        {
        case PROFILE_EVT_SRV_REG_COMPLETE:// srv register result event.
            APP_PRINT_INFO1("PROFILE_EVT_SRV_REG_COMPLETE: result %d",
                            p_param->event_data.service_reg_result);
            break;

        case PROFILE_EVT_SEND_DATA_COMPLETE:
            APP_PRINT_INFO5("PROFILE_EVT_SEND_DATA_COMPLETE: conn_id %d, cause 0x%x, service_id %d, attrib_idx 0x%x, credits %d",
                            p_param->event_data.send_data_result.conn_id,
                            p_param->event_data.send_data_result.cause,
                            p_param->event_data.send_data_result.service_id,
                            p_param->event_data.send_data_result.attrib_idx,
                            p_param->event_data.send_data_result.credits);
            if (p_param->event_data.send_data_result.cause == GAP_SUCCESS)
            {
                APP_PRINT_INFO0("PROFILE_EVT_SEND_DATA_COMPLETE success");
                if (IC_DESC_ATTR_IDX == p_param->event_data.send_data_result.attrib_idx)
                    ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_INDICATE_DONE, 0, NULL);
            }
            else
            {
                APP_PRINT_ERROR0("PROFILE_EVT_SEND_DATA_COMPLETE failed");
            }
            break;

        default:
            break;
        }
    }
	else if (service_id == breeze_srv_id)
	{
		T_BREEZE_CALLBACK_DATA *p_breeze_cb_data = (T_BREEZE_CALLBACK_DATA *)p_data;
		switch (p_breeze_cb_data->msg_type)
        {
        case SERVICE_CALLBACK_TYPE_READ_CHAR_VALUE:
			{
				switch (p_breeze_cb_data->msg_data.read_value_index)
				{
				case BREEZE_READ_1:
					{
                    	uint8_t value = 0xaa;
                    	APP_PRINT_INFO0("BREEZE_READ_1");
                    	breeze_set_parameter(BREEZE_PARAM_V1_READ_CHAR_VAL, 1, &value);
                	}
					break;

			   	case BREEZE_READ_2:
                	{
                    	uint8_t value = 0xbb;
	                    APP_PRINT_INFO0("BREEZE_READ_2");
                    	breeze_set_parameter(BREEZE_PARAM_V1_READ_CHAR_VAL, 1, &value);
                	}
			   		break;

			   	case BREEZE_READ_3:
                	{
                    	uint8_t value = 0xcc;
	                    APP_PRINT_INFO0("BREEZE_READ_3");
    	                breeze_set_parameter(BREEZE_PARAM_V1_READ_CHAR_VAL, 1, &value);
        	        }
					break;

			    case BREEZE_READ_4:
                	{
                    	uint8_t value = 0xdd;
	                    APP_PRINT_INFO0("BREEZE_READ_4");
    	                breeze_set_parameter(BREEZE_PARAM_V1_READ_CHAR_VAL, 1, &value);
        	        }
					break;

			    case BREEZE_READ_5:
                	{
                    	uint8_t value = 0xee;
                    	APP_PRINT_INFO0("BREEZE_READ_5");
                    	breeze_set_parameter(BREEZE_PARAM_V1_READ_CHAR_VAL, 1, &value);
                	}
					break;

			    default:
			   		break;
				}
        	}
        	break;
        
		case SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE:
			{
				switch (p_breeze_cb_data->msg_data.write.opcode)
				{
				case BREEZE_WRITE_REQ:
                    APP_PRINT_INFO2("BREEZE_WRITE: write type %d, len %d", p_breeze_cb_data->msg_data.write.write_type,
                                        p_breeze_cb_data->msg_data.write.len);
					//ais_service_cb_info->write_ais_wc((void *)p_breeze_cb_data->msg_data.write.p_value,
					//					p_breeze_cb_data->msg_data.write.len);
					ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_WRITE_WC, p_breeze_cb_data->msg_data.write.len, (void *)p_breeze_cb_data->msg_data.write.p_value);
					break;
				case BREEZE_WRITE_NORSP:
                    APP_PRINT_INFO2("BREEZE_WRITE: write type %d, len %d", p_breeze_cb_data->msg_data.write.write_type,
                                        p_breeze_cb_data->msg_data.write.len);
					//ais_service_cb_info->write_ais_wwnrc((void *)p_breeze_cb_data->msg_data.write.p_value,
					//					p_breeze_cb_data->msg_data.write.len);
					ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_WRITE_WWNRC, p_breeze_cb_data->msg_data.write.len, (void *)p_breeze_cb_data->msg_data.write.p_value);
					break;
 				default:
                    break;
				}
			}			
        	break;
		
        case SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION:
            {
                switch (p_breeze_cb_data->msg_data.notification_indication_index)
                {
                case BREEZE_NOTIFY_ENABLE:
                    {
                        APP_PRINT_INFO0("BREEZE_NOTIFY_ENABLE");
						//ais_service_cb_info->ais_nc_ccc_cfg_changed(BREEZE_NOTIFY_ENABLE);
						ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_NC_CCC_CFG_CHANGED, BREEZE_NOTIFY_ENABLE, NULL);
                    }
                    break;

				case BREEZE_INDICATE_ENABLE:
                    {                        
						APP_PRINT_INFO0("BREEZE_INDICATE_ENABLE");
						//ais_service_cb_info->ais_ic_ccc_cfg_changed(BREEZE_INDICATE_ENABLE);
						ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_IC_CCC_CFG_CHANGED, BREEZE_INDICATE_ENABLE, NULL);
                    }
                    break;

				case BREEZE_NOTIFY_DISABLE:
					{					    
						APP_PRINT_INFO0("BREEZE_NOTIFY_DISABLE");
						//ais_service_cb_info->ais_nc_ccc_cfg_changed(BREEZE_NOTIFY_DISABLE);
						ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_NC_CCC_CFG_CHANGED, BREEZE_NOTIFY_DISABLE, NULL);
                    }
					break;

				case BREEZE_INDICATE_DISABLE:
					{					    
						APP_PRINT_INFO0("BREEZE_INDICATE_DISABLE");
						//ais_service_cb_info->ais_ic_ccc_cfg_changed(BREEZE_INDICATE_DISABLE);
						ble_breeze_callback_send_msg(BT_BREEZE_CALLBACK_MSG_IC_CCC_CFG_CHANGED, BREEZE_INDICATE_DISABLE, NULL);
                    }
					break;
					
                default:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }

	return app_result;
}

/** @} */ /* End of group BREEZE_SEVER_CALLBACK */
/** @} */ /* End of group BREEZE_APP */
#endif
