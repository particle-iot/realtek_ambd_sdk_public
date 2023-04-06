/**
*****************************************************************************************
*     Copyright(c) 2017, Realtek Semiconductor Corporation. All rights reserved.
*****************************************************************************************
   * @file      main.c
   * @brief     Source file for BLE scatternet project, mainly used for initialize modules
   * @author    jane
   * @date      2017-06-12
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
#include <os_sched.h>
#include <string.h>
#include <bt_distance_detector_app_task.h>
#include <trace_app.h>
#include <gap.h>
#include <gap_bond_le.h>
#include <gap_scan.h>
#include <gap_adv.h>
#include <gap_config.h>
#include <gap_msg.h>
#include <bt_distance_detector_app.h>
#include <bt_distance_detector_app_flags.h>
#include "trace_uart.h"
#include <bte.h>
#include "wifi_constants.h"
#include <wifi_conf.h>
#include "rtk_coex.h"
#include <stdio.h>

/** @defgroup  SCATTERNET_DEMO_MAIN Scatternet Main
    * @brief Main file to initialize hardware and BT stack and start task scheduling
    * @{
    */

/*============================================================================*
 *                              Constants
 *============================================================================*/
/** @brief Default scan interval (units of 0.625ms, 0x10=2.5ms) */
#define DEFAULT_SCAN_INTERVAL     400 //250ms
/** @brief Default scan window (units of 0.625ms, 0x10=2.5ms) */
#define DEFAULT_SCAN_WINDOW       320 //200ms

/** @brief  Default minimum advertising interval when device is discoverable (units of 625us, 160=100ms) */
#define DEFAULT_ADVERTISING_INTERVAL_MIN            240 //150ms
/** @brief  Default maximum advertising interval */
#define DEFAULT_ADVERTISING_INTERVAL_MAX            240 //150ms

/*============================================================================*
 *                              Variables
 *============================================================================*/
/** @brief  GAP - scan response data (max size = 31 bytes) */
static const uint8_t i_beacon_scan_rsp_data[] =
{
    /* Manufacturer Specific */
    0x1A,             /* length */
    GAP_ADTYPE_MANUFACTURER_SPECIFIC,
    0x4C, 0x00,       /* Company: Apple */
    0x02,             /* Type: iBeacon */
    0x15,             /* iBeacon data length 0x15 (21) = UUID (16) + major (2) + minor (2) + RSSI (1) */
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, /* UUID (example) */
    0x00, 0x7B,       /* major (example: 123)*/
    0x01, 0xC8,       /* minor (example: 456)*/
    0xC3,             /* rssi: (example: -61 dBm) */
};

/** @brief  GAP - Advertisement data (max size = 31 bytes, best kept short to conserve power) */
static const uint8_t i_beacon_adv_data[] =
{
    /* Flags */
    0x02,             /* length */
    GAP_ADTYPE_FLAGS, /* type="Flags" */
    GAP_ADTYPE_FLAGS_GENERAL | GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED,
    /* Manufacturer Specific */
    0x1A,             /* length */
    GAP_ADTYPE_MANUFACTURER_SPECIFIC,
    0x4C, 0x00,       /* Company: Apple */
    0x02,             /* Type: iBeacon */
    0x15,             /* iBeacon data length 0x15 (21) = UUID (16) + major (2) + minor (2) + RSSI (1) */
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF, /* 16 byte UUID (example) */
    0x00, 0x7B,       /* major (example: 123)*/
    0x01, 0xC8,       /* minor (example: 456)*/
    0xC3,             /* rssi: (example: -61 dBm) */
};

/*============================================================================*
 *                              Functions
 *============================================================================*/
/**
  * @brief  Config bt stack related feature
  * @return void
  */
void bt_distance_detector_bt_stack_config_init(void)
{
    gap_config_max_le_link_num(BT_DISTANCE_DETECTOR_APP_MAX_LINKS);
    gap_config_max_le_paired_device(BT_DISTANCE_DETECTOR_APP_MAX_LINKS);  
}

/**
  * @brief  Initialize gap related parameters
  * @return void
  */
void bt_distance_detector_app_le_gap_init(void)
{
    /* Device name and device appearance */
    uint8_t  device_name[GAP_DEVICE_NAME_LEN] = "BT_DISTANCE_DETECTOR";
    uint16_t appearance = GAP_GATT_APPEARANCE_UNKNOWN;

    /* Scan parameters */
    uint8_t  scan_mode = GAP_SCAN_MODE_PASSIVE;
    uint16_t scan_interval = DEFAULT_SCAN_INTERVAL;
    uint16_t scan_window = DEFAULT_SCAN_WINDOW;
    uint8_t  scan_filter_policy = GAP_SCAN_FILTER_ANY;
    uint8_t  scan_filter_duplicate = GAP_SCAN_FILTER_DUPLICATE_DISABLE;

    /* Advertising parameters */
    uint8_t  adv_evt_type = GAP_ADTYPE_ADV_NONCONN_IND;
    uint8_t  adv_direct_type = GAP_REMOTE_ADDR_LE_PUBLIC;
    uint8_t  adv_direct_addr[GAP_BD_ADDR_LEN] = {0};
    uint8_t  adv_chann_map = GAP_ADVCHAN_ALL;
    uint8_t  adv_filter_policy = GAP_ADV_FILTER_ANY;
    uint16_t adv_int_min = DEFAULT_ADVERTISING_INTERVAL_MIN;
    uint16_t adv_int_max = DEFAULT_ADVERTISING_INTERVAL_MAX;

    /* Set device name and device appearance */
    le_set_gap_param(GAP_PARAM_DEVICE_NAME, GAP_DEVICE_NAME_LEN, device_name);
    le_set_gap_param(GAP_PARAM_APPEARANCE, sizeof(appearance), &appearance);

    /* Set scan parameters */
    le_scan_set_param(GAP_PARAM_SCAN_MODE, sizeof(scan_mode), &scan_mode);
    le_scan_set_param(GAP_PARAM_SCAN_INTERVAL, sizeof(scan_interval), &scan_interval);
    le_scan_set_param(GAP_PARAM_SCAN_WINDOW, sizeof(scan_window), &scan_window);
    le_scan_set_param(GAP_PARAM_SCAN_FILTER_POLICY, sizeof(scan_filter_policy),
                      &scan_filter_policy);
    le_scan_set_param(GAP_PARAM_SCAN_FILTER_DUPLICATES, sizeof(scan_filter_duplicate),
                      &scan_filter_duplicate);

    /* Set advertising parameters */
    le_adv_set_param(GAP_PARAM_ADV_EVENT_TYPE, sizeof(adv_evt_type), &adv_evt_type);
    le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR_TYPE, sizeof(adv_direct_type), &adv_direct_type);
    le_adv_set_param(GAP_PARAM_ADV_DIRECT_ADDR, sizeof(adv_direct_addr), adv_direct_addr);
    le_adv_set_param(GAP_PARAM_ADV_CHANNEL_MAP, sizeof(adv_chann_map), &adv_chann_map);
    le_adv_set_param(GAP_PARAM_ADV_FILTER_POLICY, sizeof(adv_filter_policy), &adv_filter_policy);
    le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MIN, sizeof(adv_int_min), &adv_int_min);
    le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MAX, sizeof(adv_int_max), &adv_int_max);
    le_adv_set_param(GAP_PARAM_ADV_DATA, sizeof(i_beacon_adv_data), (void *)i_beacon_adv_data);
    le_adv_set_param(GAP_PARAM_SCAN_RSP_DATA, sizeof(i_beacon_scan_rsp_data), (void *)i_beacon_scan_rsp_data);

    /* register gap message callback */
    le_register_app_cb(bt_distance_detector_app_gap_callback);

#if F_BT_LE_5_0_SET_PHY_SUPPORT
    uint8_t  phys_prefer = GAP_PHYS_PREFER_ALL;
    uint8_t  tx_phys_prefer = GAP_PHYS_PREFER_1M_BIT | GAP_PHYS_PREFER_2M_BIT;
    uint8_t  rx_phys_prefer = GAP_PHYS_PREFER_1M_BIT | GAP_PHYS_PREFER_2M_BIT;
    le_set_gap_param(GAP_PARAM_DEFAULT_PHYS_PREFER, sizeof(phys_prefer), &phys_prefer);
    le_set_gap_param(GAP_PARAM_DEFAULT_TX_PHYS_PREFER, sizeof(tx_phys_prefer), &tx_phys_prefer);
    le_set_gap_param(GAP_PARAM_DEFAULT_RX_PHYS_PREFER, sizeof(rx_phys_prefer), &rx_phys_prefer);
#endif
}

/**
 * @brief  Add GATT services, clients and register callbacks
 * @return void
 */
void bt_distance_detector_app_le_profile_init(void)
{

}

/**
 * @brief    Contains the initialization of all tasks
 * @note     There is only one task in BLE Scatternet APP, thus only one APP task is init here
 * @return   void
 */
void bt_distance_detector_task_init(void)
{
    bt_distance_detector_app_task_init();
    bt_distance_detector_transfer_task_init();
}

/**
 * @brief    Entry of APP code
 * @return   int (To avoid compile warning)
 */
int bt_distance_detector_main(void)
{
    bt_trace_init();
    bt_distance_detector_bt_stack_config_init();
    bte_init();
    le_gap_init(BT_DISTANCE_DETECTOR_APP_MAX_LINKS);
    bt_distance_detector_app_le_gap_init();
    bt_distance_detector_app_le_profile_init();
    bt_distance_detector_task_init();

    return 0;
}

int bt_distance_detector_app_init(void)
{
	T_GAP_DEV_STATE new_state;

	/*Wait WIFI init complete*/
	while(!(wifi_is_up(RTW_STA_INTERFACE) || wifi_is_up(RTW_AP_INTERFACE))) {
		os_delay(1000);
	}

	//judge BT DISTANCE DETECTOR is already on
	le_get_gap_param(GAP_PARAM_DEV_STATE , &new_state);
	if (new_state.gap_init_state == GAP_INIT_STATE_STACK_READY) {
		//bt_stack_already_on = 1;
		printf("[BT DISTANCE DETECTOR]BT Stack already on\r\n");
		return 0;
	}
	else
		bt_distance_detector_main();

	bt_coex_init();

	/*Wait BT init complete*/
	do {
		os_delay(100);
		le_get_gap_param(GAP_PARAM_DEV_STATE , &new_state);
	}while(new_state.gap_init_state != GAP_INIT_STATE_STACK_READY);

	return 0;
}

void bt_distance_detector_app_deinit(void)
{
	bt_distance_detector_app_task_deinit();
	bt_distance_detector_transfer_task_deinit();

	T_GAP_DEV_STATE state;
	le_get_gap_param(GAP_PARAM_DEV_STATE , &state);
	if (state.gap_init_state != GAP_INIT_STATE_STACK_READY) {
		printf("[BT DISTANCE DETECTOR]BT Stack is not running\r\n");
	}
#if F_BT_DEINIT
	else {
		bte_deinit();
		bt_trace_uninit();
		printf("[BT DISTANCE DETECTOR]BT Stack deinitalized\r\n");
	}
#endif
}
/** @} */ /* End of group SCATTERNET_DEMO_MAIN */
#endif