/**
*********************************************************************************************************
*               Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
**********************************************************************************************************
* @file     breeze.c
* @brief    Breeze service source file.
* @details  Interfaces to access Breeze service.
* @author
* @date
* @version  v1.0
*********************************************************************************************************
*/
#include <platform_opts_bt.h>
#if defined(CONFIG_BT_BREEZE) && CONFIG_BT_BREEZE
#include "stdint.h"
#include "stddef.h"
#include "string.h"
#include "trace_app.h"
#include "profile_server.h"

#include "breeze.h"
#include "breeze_hal_ble.h"

/********************************************************************************************************
* local static variables defined here, only used in this source file.
********************************************************************************************************/

#define GATT_UUID_AIS_SERVICE	0xFE83   // BLE_UUID_AIS_SERVICE

static P_FUN_SERVER_GENERAL_CB pfn_breeze_cb = NULL;
ind_cb g_indicate_cb = NULL;
static uint8_t breeze_read_value = 0;

extern T_ATTRIB_APPL attrs[AIS_ATTR_NUM];

bool breeze_set_parameter(T_BREEZE_PARAM_TYPE param_type, uint16_t len, uint8_t *p_value)
{
    bool ret = true;

    switch (param_type)
    {
    default:
        ret = false;
   		break;
    case BREEZE_PARAM_V1_READ_CHAR_VAL:
		{
        	if (len != sizeof(uint8_t))
        	{
        		ret = false;
        	}
        	else
        	{
            	breeze_read_value = *p_value;
        	}
    	}
		break;
    }

    if (!ret)
    {
        APP_PRINT_ERROR0("breeze_set_parameter failed");
    }

    return ret;
}


/**
  * @brief send notification of breeze notify characteristic value.
  *
  * @param[in] conn_id           connection id
  * @param[in] service_id        service ID of service.
  * @param[in] p_value           characteristic value to notify
  * @param[in] length            characteristic value length to notify
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */
bool breeze_send_notify(uint8_t conn_id, T_SERVER_ID service_id, void *p_value,
                                     uint16_t length)
{
    APP_PRINT_INFO0("breeze_send_notify");
    // send notification to client
    return server_send_data(conn_id, service_id, NC_DESC_ATTR_IDX, p_value,
                            length, GATT_PDU_TYPE_NOTIFICATION);
}


/**
  * @brief send indication of breeze indicate characteristic value.
  *
  * @param[in] conn_id           connection id
  * @param[in] service_id        service ID of service.
  * @param[in] p_value           characteristic value to notify
  * @param[in] length            characteristic value length to notify
  * @return notification action result
  * @retval 1 true
  * @retval 0 false
  */

bool breeze_send_indicate(uint8_t conn_id, T_SERVER_ID service_id, void *p_value,
                                       uint16_t length, ind_cb cb)
{
	bool ret;
    APP_PRINT_INFO0("breeze_send_indicate");
	g_indicate_cb = cb;
    // send indication to client
    ret = server_send_data(conn_id, service_id, IC_DESC_ATTR_IDX, p_value,
    						length, GATT_PDU_TYPE_INDICATION);
	return ret;
}

T_APP_RESULT breeze_attr_write_cb(uint8_t conn_id, T_SERVER_ID service_id,
                                            uint16_t attrib_index, T_WRITE_TYPE write_type, uint16_t length, uint8_t *p_value,
                                            P_FUN_WRITE_IND_POST_PROC *p_write_ind_post_proc)
{
    T_BREEZE_CALLBACK_DATA callback_data;
    T_APP_RESULT  cause = APP_RESULT_SUCCESS;
    APP_PRINT_INFO1("breeze_attr_write_cb write_type = 0x%x", write_type);
	
    if ((WC_DESC_ATTR_IDX == attrib_index && WRITE_REQUEST == write_type)||
		(WWNRC_DESC_ATTR_IDX == attrib_index && WRITE_WITHOUT_RESPONSE == write_type))
    {
        /* Make sure written value size is valid. */
        if (p_value == NULL)
        {
            cause  = APP_RESULT_INVALID_VALUE_SIZE;
        }
        else
        {
            /* Notify Application. */
            callback_data.msg_type = SERVICE_CALLBACK_TYPE_WRITE_CHAR_VALUE;
            callback_data.conn_id  = conn_id;
            callback_data.msg_data.write.opcode = ((write_type == WRITE_REQUEST)? BREEZE_WRITE_REQ : BREEZE_WRITE_NORSP);
            callback_data.msg_data.write.write_type = write_type;
            callback_data.msg_data.write.len = length;
            callback_data.msg_data.write.p_value = p_value; 
			
            if (pfn_breeze_cb)
            {
                pfn_breeze_cb(service_id, (void *)&callback_data);
            }
        }
    }
	else
    {
        APP_PRINT_ERROR2("breeze_attr_write_cb Error: attrib_index 0x%x, length %d", attrib_index,length);
        cause = APP_RESULT_ATTR_NOT_FOUND;
    }
    return cause;
}


/**
 * @brief update CCCD bits from stack.
 *
 * @param conn_id           Connection ID.
 * @param service_id        Service ID.
 * @param index             Attribute index of characteristic data.
 * @param ccc_bits          CCCD bits from stack.
 * @return None
*/
void breeze_cccd_update_cb(uint8_t conn_id, T_SERVER_ID service_id, uint16_t index, uint16_t ccc_bits)
{
    T_BREEZE_CALLBACK_DATA callback_data;
    callback_data.msg_type = SERVICE_CALLBACK_TYPE_INDIFICATION_NOTIFICATION;
    callback_data.conn_id = conn_id;
    bool handle = false;
    APP_PRINT_INFO2("breeze_cccd_update_cb index = %d ccc_bits %x", index, ccc_bits);
	
	if(NC_CCC_ATTR_IDX == index)
	{
		handle = true; 
		if(ccc_bits & GATT_CLIENT_CHAR_CONFIG_NOTIFY)
		{
			callback_data.msg_data.notification_indication_index = BREEZE_NOTIFY_ENABLE;
		}
		else	
		{	
			callback_data.msg_data.notification_indication_index = BREEZE_NOTIFY_DISABLE;
		}
	}
	else if(IC_CCC_ATTR_IDX == index)
	{
		handle = true; 
		if(ccc_bits & GATT_CLIENT_CHAR_CONFIG_INDICATE)
		{
			callback_data.msg_data.notification_indication_index = BREEZE_INDICATE_ENABLE;
		}
		else
		{
			callback_data.msg_data.notification_indication_index = BREEZE_INDICATE_DISABLE;
		}
	}
		
	if (pfn_breeze_cb && (handle == true))
    {
        pfn_breeze_cb(service_id, (void *)&callback_data);
    }
	
    return;
}

T_APP_RESULT breeze_attr_read_cb(uint8_t conn_id, T_SERVER_ID service_id, uint16_t attrib_index,
                              uint16_t offset, uint16_t *p_length, uint8_t **pp_value)

{
    T_APP_RESULT cause = APP_RESULT_SUCCESS;
    *p_length = 0;
	T_BREEZE_CALLBACK_DATA callback_data;
    callback_data.msg_type = SERVICE_CALLBACK_TYPE_READ_CHAR_VALUE;
    callback_data.conn_id = conn_id;

    APP_PRINT_INFO2("breeze_attr_read_cb attrib_index = %d offset %x", attrib_index, offset);

    switch (attrib_index)
    {
    default:
        {
            APP_PRINT_ERROR1("breeze_attr_read_cb attrib_index = %d not found", attrib_index);
            cause  = APP_RESULT_ATTR_NOT_FOUND;
        }
        break;

	case RC_DESC_ATTR_IDX:
        {
            callback_data.msg_data.read_value_index = BREEZE_READ_1;
            cause = pfn_breeze_cb(service_id, (void *)&callback_data);

            *pp_value = &breeze_read_value;
            *p_length = sizeof(breeze_read_value);
        }
        break;
		
    case WC_DESC_ATTR_IDX:
        {
            callback_data.msg_data.read_value_index = BREEZE_READ_2;
            cause = pfn_breeze_cb(service_id, (void *)&callback_data);

            *pp_value = &breeze_read_value;
            *p_length = sizeof(breeze_read_value);
        }
        break;

	case IC_DESC_ATTR_IDX:
        {
            callback_data.msg_data.read_value_index = BREEZE_READ_3;
            cause = pfn_breeze_cb(service_id, (void *)&callback_data);

            *pp_value = &breeze_read_value;
            *p_length = sizeof(breeze_read_value);
        }
        break;

	case WWNRC_DESC_ATTR_IDX:
        {
            callback_data.msg_data.read_value_index = BREEZE_READ_4;
            cause = pfn_breeze_cb(service_id, (void *)&callback_data);

            *pp_value = &breeze_read_value;
            *p_length = sizeof(breeze_read_value);
        }
        break;

	case NC_DESC_ATTR_IDX:
        {
            callback_data.msg_data.read_value_index = BREEZE_READ_5;
            cause = pfn_breeze_cb(service_id, (void *)&callback_data);

            *pp_value = &breeze_read_value;
            *p_length = sizeof(breeze_read_value);
        }
        break;
    }
	
    return (cause);
}


/**
 * @brief Breeze Service Callbacks.
*/
const T_FUN_GATT_SERVICE_CBS breeze_cbs =
{
    breeze_attr_read_cb,  // Read callback function pointer
    breeze_attr_write_cb, // Write callback function pointer
    breeze_cccd_update_cb  // CCCD update callback function pointer
};

/**
  * @brief       Add breeze service to the BLE stack database.
  *
  *
  * @param[in]   p_func  Callback when service attribute was read, write or cccd update.
  * @return Service id generated by the BLE stack: @ref T_SERVER_ID.
  * @retval 0xFF Operation failure.
  * @retval Others Service id assigned by stack.
  *
  * <b>Example usage</b>
  * \code{.c}
     void profile_init()
     {
         server_init(1);
         breeze_id = breeze_add_service(app_handle_profile_message);
     }
  * \endcode
  */
T_SERVER_ID breeze_add_service(void *p_func)
{
    T_SERVER_ID service_id;
    if (false == server_add_service(&service_id,
                                    (uint8_t *)attrs,
                                    sizeof(attrs),
                                    breeze_cbs))
    {
        APP_PRINT_ERROR1("breeze_add_service: service_id %d", service_id);
        service_id = 0xff;
    }
    pfn_breeze_cb = (P_FUN_SERVER_GENERAL_CB)p_func;
    return service_id;
}
#endif
