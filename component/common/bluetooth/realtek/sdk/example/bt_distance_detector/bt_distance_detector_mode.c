#include <platform_opts_bt.h>
#if defined(CONFIG_BT_DISTANCE_DETECTOR) && CONFIG_BT_DISTANCE_DETECTOR
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <gap_scan.h>
#include <gap_msg.h>
#include <gap_adv.h>
#include <os_sched.h>
#include <os_timer.h>
#include <bt_distance_detector_mode.h>
#include <bt_distance_detector_app_task.h>

/*================================================================================================*
 *                                         Constants
 *================================================================================================*/
/** @brief Normal mode scan interval (units of 0.625ms, 0x10=2.5ms) */
#define NORMAL_MODE_SCAN_INTERVAL     400 //250ms
/** @brief Normal mode scan window (units of 0.625ms, 0x10=2.5ms) */
#define NORMAL_MODE_SCAN_WINDOW       320 //200ms

/** @brief  Normal mode minimum advertising interval when device is discoverable (units of 625us, 160=100ms) */
#define NORMAL_MODE_ADVERTISING_INTERVAL_MIN            240 //150ms
/** @brief  Normal mode maximum advertising interval */
#define NORMAL_MODE_ADVERTISING_INTERVAL_MAX            240 //150ms

/** @brief High detection mode scan interval (units of 0.625ms, 0x10=2.5ms) */
#define HIGH_DETETION_MODE_SCAN_INTERVAL     240 //150ms
/** @brief High detection mode scan window (units of 0.625ms, 0x10=2.5ms) */
#define HIGH_DETETION_MODE_SCAN_WINDOW       160 //100ms

/** @brief  High detection mode minimum advertising interval when device is discoverable (units of 625us, 160=100ms) */
#define HIGH_DETETION_MODE_ADVERTISING_INTERVAL_MIN            160 //100ms
/** @brief  High detection mode maximum advertising interval */
#define HIGH_DETETION_MODE_ADVERTISING_INTERVAL_MAX            160 //100ms

#define BEACON_NUM_THRESHOLD_VALUE  5  //max beacon num within 500ms

#define MAX_BEACON_LOSS_COUNT  2000   // 2000ms(2s) -- the time that device can loss beacon
#define DEVICE_SLEEP_DURATION  1000   // 1000ms(1s) -- the device sleep time if receive no beacon within MAX_BEACON_LOSS_COUNT
#define DEVICE_HALT_TIME       60     // 60s -- if receive no beacon DEVICE_HALT_TIME, then need clear flash

/*================================================================================================*
 *                                         Variables
 *================================================================================================*/
void *low_power_mode_timer;         //low power mode timer
void *high_datection_mode_timer;    //high detection mode timer (500ms)
void *beacon_num_check_timer;       //the timer: if scan no ibeacons within 60s then clear the node info of FTL

device_mode_t device_mode;

/*===============================================================================================*
 *                                        Functions
 *===============================================================================================*/
void update_advertising_and_scan_parameters(uint16_t scan_interval, uint16_t scan_window, 
									uint16_t adv_int_max, uint16_t adv_int_min)
{
	T_GAP_DEV_STATE new_state = {0};

	le_get_gap_param(GAP_PARAM_DEV_STATE , &new_state);

	if (new_state.gap_scan_state == GAP_SCAN_STATE_SCANNING)
	{
		bt_distance_detector_send_msg_to_apptask(2); //send msg stop scan
	}

	if (new_state.gap_adv_state == GAP_ADV_STATE_ADVERTISING)
	{
		bt_distance_detector_send_msg_to_apptask(0); //send msg stop adv
	}

	do {
		os_delay(1);
		le_get_gap_param(GAP_PARAM_DEV_STATE , &new_state);
	} while ((new_state.gap_adv_state != GAP_ADV_STATE_IDLE) || (new_state.gap_scan_state != GAP_SCAN_STATE_IDLE));

	le_scan_set_param(GAP_PARAM_SCAN_INTERVAL, sizeof(scan_interval), &scan_interval);
	le_scan_set_param(GAP_PARAM_SCAN_WINDOW, sizeof(scan_window), &scan_window);

	le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MAX, sizeof(adv_int_max), &adv_int_max);
	le_adv_set_param(GAP_PARAM_ADV_INTERVAL_MIN, sizeof(adv_int_min), &adv_int_min);

	bt_distance_detector_send_msg_to_apptask(1); //send msg start adv
	bt_distance_detector_send_msg_to_apptask(3); //send msg start scan
	do {
		os_delay(1);
		le_get_gap_param(GAP_PARAM_DEV_STATE , &new_state);
	} while ((new_state.gap_adv_state != GAP_ADV_STATE_ADVERTISING) || (new_state.gap_scan_state != GAP_SCAN_STATE_SCANNING));

}

/* @param[in] num scan iBeacon adv numbers
 * @return 0-not update adv&scan parameters, 1-update adv&scan parameters
 */
int device_enter_high_detection_mode(uint16_t num)
{
	if (num > BEACON_NUM_THRESHOLD_VALUE)
	{
		if (device_mode.device_mode_flag != HIGH_DETECTION_MODE)
		{
			printf("device enter high detection mode\r\n");

			device_mode.device_mode_flag = HIGH_DETECTION_MODE;

			uint16_t scan_interval = HIGH_DETETION_MODE_SCAN_INTERVAL;
			uint16_t scan_window = HIGH_DETETION_MODE_SCAN_WINDOW;
			uint16_t adv_int_max = HIGH_DETETION_MODE_ADVERTISING_INTERVAL_MAX;
			uint16_t adv_int_min = HIGH_DETETION_MODE_ADVERTISING_INTERVAL_MIN;

			update_advertising_and_scan_parameters(scan_interval, scan_window, adv_int_max, adv_int_min);

			return 1;
		}
	}

	device_mode.high_timer_flag = 1; //if not update high detection mode parameters, set flag=1 t0 restart high timer
	return 0;
}

int device_enter_normal_mode(void)
{
	printf("device enter normal mode\r\n");
	int ret = 0;
	uint16_t scan_interval = NORMAL_MODE_SCAN_INTERVAL;
	uint16_t scan_window = NORMAL_MODE_SCAN_WINDOW;
	uint16_t adv_int_max = NORMAL_MODE_ADVERTISING_INTERVAL_MAX;
	uint16_t adv_int_min = NORMAL_MODE_ADVERTISING_INTERVAL_MIN;

	update_advertising_and_scan_parameters(scan_interval, scan_window, adv_int_max, adv_int_min);

	return ret;
}

/* ret=1: no beacons detected within 2s */
int detect_beacons_within_two_second(void)
{
	int ret = 0;
	uint8_t time_count = 0;
	uint8_t two_seconds_detect_state = device_mode.two_second.current - device_mode.two_second.previous;

	if (device_mode.beacon_count != 0)
	{
		device_mode.time_step = 0;
		device_mode.two_second.previous = device_mode.two_second.current;
	}
	else
	{
		device_mode.time_step ++;

		time_count = (MAX_BEACON_LOSS_COUNT % 500) ? (MAX_BEACON_LOSS_COUNT / 500 + 1) : (MAX_BEACON_LOSS_COUNT / 500);

		/* detect by 2s */
		if (device_mode.time_step == time_count) // if Ibeacons are not detected for 4 consecutive times
		{
			device_mode.time_step = 0;
			device_mode.two_second.previous = device_mode.two_second.current;
			if (two_seconds_detect_state == 0)
			{
				printf("device enter low power mode\r\n");
				device_mode.low_power_timer_flag = 1; //set flag=1 to start low power timer
				device_mode.device_mode_flag = LOW_POWER_MODE;

				/* in this example, we just stop scan and adv as the device enter low power mode */
				bt_distance_detector_send_msg_to_apptask(2); //send msg stop scan
				bt_distance_detector_send_msg_to_apptask(0); //send msg stop adv

				ret = 1;
			}
		}
	}

	return ret;
}

void high_detection_mode_timer_callback(void *pxTimer)
{
	//printf("%s: beacon_counter %d\r\n", __FUNCTION__, device_mode.beacon_count);//debug
	if (detect_beacons_within_two_second() == 0)
	{
		device_enter_high_detection_mode(device_mode.beacon_count);
		if (device_mode.high_timer_flag == 1)
		{
			device_mode.high_timer_flag = 0;
			os_timer_start(&high_datection_mode_timer);
		}
	}
	device_mode.beacon_count = 0;
}

void low_power_mode_timer_callback(void *pxTimer)
{
	//printf("%s\r\n", __FUNCTION__);//debug
	memset(&device_mode, 0, sizeof(device_mode_t));
	device_enter_normal_mode();
}

void beacon_num_check_timer_callback(void *pxTimer)
{
	//printf("%s\r\n", __FUNCTION__); //debug
	device_msg_t dev_info;
	memset(&dev_info, 0, sizeof(device_msg_t));
	dev_info.flag = 1; //send msg from 60s-timer flag

	bt_distance_detector_send_msg_to_transfertask(dev_info);
}

void bt_distance_detector_timer_init(void)
{
	memset(&device_mode, 0, sizeof(device_mode_t));

	os_timer_create(&high_datection_mode_timer, "high_detection_mode_timer", 1, 500, false, high_detection_mode_timer_callback);
	os_timer_create(&low_power_mode_timer, "low_power_mode_timer", 1, DEVICE_SLEEP_DURATION, false, low_power_mode_timer_callback);
	os_timer_create(&beacon_num_check_timer, "beacon_num_check_timer", 1, DEVICE_HALT_TIME * 1000, false, beacon_num_check_timer_callback);
}

void bt_distance_detector_timer_deinit(void)
{
	if (high_datection_mode_timer) {
		os_timer_delete(&high_datection_mode_timer);
	}
	if (low_power_mode_timer) {
		os_timer_delete(&low_power_mode_timer);
	}
	if (beacon_num_check_timer) {
		os_timer_delete(&beacon_num_check_timer);
	}

	high_datection_mode_timer = NULL;
	low_power_mode_timer = NULL;
	beacon_num_check_timer = NULL;

	memset(&device_mode, 0, sizeof(device_mode_t));
}

#endif
