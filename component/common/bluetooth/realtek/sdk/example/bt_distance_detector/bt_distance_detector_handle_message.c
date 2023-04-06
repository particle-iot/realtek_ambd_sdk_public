#include <platform_opts_bt.h>
#if defined(CONFIG_BT_DISTANCE_DETECTOR) && CONFIG_BT_DISTANCE_DETECTOR
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <bt_distance_detector_handle_message.h>
#include <os_mem.h>
#include <ftl_app.h>

#define bt_distance_detector_flash_save(pdata, size, offset)   ftl_save(pdata, offset, size)
#define bt_distance_detector_flash_load(pdata, size, offset)   ftl_load(pdata, offset, size)
#define BT_DISTANCE_DETECTOR_MEMBER_OFFSET(struct_type, member)     ((uint32_t)&((struct_type *)0)->member)

typedef enum
{
    DETECTOR_FLASH_PARAMS_FEATURES,
    DETECTOR_FLASH_PARAMS_CLEAR,
    DETECTOR_FLASH_PARAMS_NEW_ENTRY,
    DETECTOR_FLASH_PARAMS_UPDATE,
} flash_params_type_t;

typedef struct
{
    uint16_t detector_flash_offset;
    uint16_t detector_flash_scan_tag_msg_offset; //corresponding scan_tag_msg all size
    uint16_t detector_flash_total_size;
} flash_layout_info_t;

flash_layout_info_t flash_layout_info;
scan_tag_msg_t scan_tag_msg;

/******************************************* function *********************************************************/
void bt_distance_detector_flash_init(void)
{
	flash_layout_info.detector_flash_offset = 0;
	flash_layout_info.detector_flash_scan_tag_msg_offset = flash_layout_info.detector_flash_offset + 4 + \
											scan_tag_msg.node_num * sizeof(Beacon_data_t);

}

void bt_distance_detector_flash_store(flash_params_type_t param_type, int16_t param)
{
	uint32_t ret = 0;

	switch (param_type)
	{
	case DETECTOR_FLASH_PARAMS_NEW_ENTRY:
		{
			if (BT_DISTANCE_DETECTOR_FLASH)
			{
				printf("save new entry(new device address and iBeacon info) to flash\r\n");
				uint16_t node_index = param;
				uint16_t offset = flash_layout_info.detector_flash_offset + 4 + node_index * sizeof(Beacon_data_t);
				ret = bt_distance_detector_flash_save((void *)&scan_tag_msg.sBeacon[node_index], sizeof(Beacon_data_t), offset);
			}
		}
		break;

	case DETECTOR_FLASH_PARAMS_UPDATE:
		{
			if (BT_DISTANCE_DETECTOR_FLASH)
			{
				printf("update iBeacon info in flash\r\n");
				uint16_t node_index = param;
				uint16_t mem_offset = BT_DISTANCE_DETECTOR_MEMBER_OFFSET(Beacon_data_t, previous_rssi);
				uint16_t size = sizeof(Beacon_data_t) - mem_offset;
				uint16_t offset = flash_layout_info.detector_flash_offset + 4 + node_index * sizeof(Beacon_data_t) + mem_offset;

				ret = bt_distance_detector_flash_save((void *)&scan_tag_msg.sBeacon[node_index].previous_rssi, size, offset);
			}
		}
		break;

	case DETECTOR_FLASH_PARAMS_CLEAR:
		{
			if (BT_DISTANCE_DETECTOR_FLASH)
			{
				uint16_t offset = 0;
				Beacon_data_t sBeacon_entry;
				uint16_t node_index = 0;

				if (param == -1)
				{
					printf("clear all in flash\r\n");
					for (node_index = 0; node_index < scan_tag_msg.node_num; node_index++)
					{
						offset = flash_layout_info.detector_flash_offset + 4 + node_index * sizeof(Beacon_data_t);
						uint32_t ret1 = bt_distance_detector_flash_load((void *)&sBeacon_entry, 4, offset);
						if (ret1 == 0 && sBeacon_entry.used)
						{
							sBeacon_entry.used = 0;
							ret = bt_distance_detector_flash_save((void *)&sBeacon_entry, 4, offset);

							if (ret != 0)
							{
								break;
							}
						}
					}
				}
				else
				{
					node_index = param;
					printf("clear node[%d] in flash\r\n", node_index);
					offset = flash_layout_info.detector_flash_offset + 4 + node_index * sizeof(Beacon_data_t);
					uint32_t ret1 = bt_distance_detector_flash_load((void *)&sBeacon_entry, 4, offset);
					if (ret1 == 0 && sBeacon_entry.used)
					{
						sBeacon_entry.used = 0;
						ret = bt_distance_detector_flash_save((void *)&sBeacon_entry, 4, offset);

						if (ret != 0)
						{
							break;
						}
					}
				}
			}
		}
		break;

	default:
		break;
	}

	if (ret != 0)
	{
		printf("%s: failed to store the related information, params type = %d, cause = %d\r\n", __FUNCTION__, param_type, ret);
	}

}

/* read and check the value from flash when init bt */
void bt_distance_detector_flash_restore(void)
{
	uint32_t ret = 0;

	if (BT_DISTANCE_DETECTOR_FLASH)
	{
		Beacon_data_t *sBeacon = scan_tag_msg.sBeacon;

		for (uint16_t node_index = 0; node_index < scan_tag_msg.node_num; node_index++)
		{
			uint16_t offset = flash_layout_info.detector_flash_offset + 4 + node_index * sizeof(Beacon_data_t);
			ret = bt_distance_detector_flash_load((void *)&sBeacon[node_index], sizeof(Beacon_data_t), offset);

			if (ret != 0)
			{
				sBeacon[node_index].used = 0;
			}
			else
			{
				if (sBeacon[node_index].used == 1)
				{
					uint8_t *addr = sBeacon[node_index].addr;
					printf("%s: addr 0x%02x:%02x:%02x:%02x:%02x:%02x, TxPower %d\r\n", __FUNCTION__, 
						addr[5], addr[4], addr[3], addr[2], addr[1], addr[0], sBeacon[node_index].txpower);
				}
			}
		}
	}
}

/* allocate memory and reset parameters when init bt */
void bt_distance_detector_scan_result_init(uint16_t node_num)
{
	scan_tag_msg.node_num = node_num; //scan_result.node_num = MAX_NODE_COUNT;
	if (node_num == 0)
	{
		return;
	}

	scan_tag_msg.sBeacon = os_mem_alloc(RAM_TYPE_DATA_ON, sizeof(Beacon_data_t) * node_num);

	if (scan_tag_msg.sBeacon == NULL)
	{
		printf("scan_result_init: fail to allocate memory, node num = %d\r\n", node_num);
		return;
	}
	else
	{
		for (uint16_t node_index = 0; node_index < node_num; node_index++)
		{
			memset(&scan_tag_msg.sBeacon[node_index], 0, sizeof(Beacon_data_t));
		}
	}

}

void bt_distance_detector_scan_result_deinit(void)
{
	if (scan_tag_msg.sBeacon)
	{
		os_mem_free(scan_tag_msg.sBeacon);
		scan_tag_msg.sBeacon = NULL;
	}
}

static int8_t weighted_mean(int8_t previous_rssi, int8_t current_rssi)
{
	int8_t filter_rssi = 0;
	filter_rssi = (int8_t)(((1 - DISTANCE_DETECTION_ALPHA) * current_rssi) + (DISTANCE_DETECTION_ALPHA * previous_rssi));

	return filter_rssi;
}

static int8_t Average_Filter(uint8_t node_index, uint16_t aSample_count)
{
	int16_t Total_RSSI=0;
	int8_t average_rssi = 0;

	for(uint16_t rssi_count=0; rssi_count < aSample_count ; rssi_count++)
	{
		Total_RSSI = Total_RSSI + scan_tag_msg.sBeacon[node_index].filter_rssi[rssi_count] ;
	}
	average_rssi = (int8_t)(Total_RSSI / aSample_count);

	return average_rssi;
}

/*
 * @return >=0: success
 * @return -1: invaild node number
 * @return -2: space full
*/
int bt_distance_detector_scan_result_check(uint8_t *dev_addr, int8_t rssi, int8_t txpower)
{
	uint8_t node_index = 0;
	uint8_t rssi_count = 0;
	int8_t filter_rssi = 0;

	if (scan_tag_msg.node_num == 0)
	{
		printf("%s: node_num = %d\r\n", __FUNCTION__, scan_tag_msg.node_num);
		return -1;
	}

	for (node_index = 0; node_index < scan_tag_msg.node_num; node_index++)
	{
		if ((scan_tag_msg.sBeacon[node_index].used) && (memcmp(scan_tag_msg.sBeacon[node_index].addr, dev_addr, 6) == 0))
		{
			// device has exist
			printf("scan_tag_msg.sBeacon[%d] dev_addr 0x%02x:%02x:%02x:%02x:%02x:%02x\t rssi=%d\r\n", 
							node_index, dev_addr[5], dev_addr[4], dev_addr[3], 
							dev_addr[2], dev_addr[1], dev_addr[0], rssi);

			rssi_count = scan_tag_msg.sBeacon[node_index].rssi_count;

			if (rssi_count >= BT_DISTANCE_DETECTOR_RSSI_SAMPLE_COUNT)
			{
				rssi_count = 0;
			}

			filter_rssi = weighted_mean(scan_tag_msg.sBeacon[node_index].previous_rssi, rssi);
			scan_tag_msg.sBeacon[node_index].filter_rssi[rssi_count++] = filter_rssi;
			scan_tag_msg.sBeacon[node_index].rssi_count = rssi_count;
			scan_tag_msg.sBeacon[node_index].previous_rssi = rssi;
			scan_tag_msg.sBeacon[node_index].beacon_num++; //count for 10s

			bt_distance_detector_flash_store(DETECTOR_FLASH_PARAMS_UPDATE, node_index);

			return node_index;
		}

	}

	//allocate a space for new device
	for (node_index = 0; node_index < scan_tag_msg.node_num; node_index++)
	{
		if (scan_tag_msg.sBeacon[node_index].used == 0)
		{
			printf("check as a new device\r\n");
			printf("scan_tag_msg.sBeacon[%d] dev_addr 0x%02x:%02x:%02x:%02x:%02x:%02x\t rssi=%d\r\n", 
							node_index, dev_addr[5], dev_addr[4], dev_addr[3], 
							dev_addr[2], dev_addr[1], dev_addr[0], rssi);

			memcpy(scan_tag_msg.sBeacon[node_index].addr, dev_addr, 6);
			scan_tag_msg.sBeacon[node_index].txpower = txpower;
			scan_tag_msg.sBeacon[node_index].filter_rssi[0] = rssi;
			scan_tag_msg.sBeacon[node_index].rssi_count = 1;
			scan_tag_msg.sBeacon[node_index].used = 1;
			scan_tag_msg.sBeacon[node_index].previous_rssi = rssi;
			scan_tag_msg.sBeacon[node_index].beacon_num = 1; //count for 10s

			bt_distance_detector_flash_store(DETECTOR_FLASH_PARAMS_NEW_ENTRY, node_index);

			return node_index;
		}
	}

	printf("%s: space is full\r\n", __FUNCTION__);

	return -2; //the space is full
}

void bt_distance_detector_scan_result_clear(void)
{
	if (scan_tag_msg.node_num == 0)
	{
		return;
	}

	/** clear all the records */
	for (uint16_t node_index = 0; node_index < scan_tag_msg.node_num; node_index++)
	{
		scan_tag_msg.sBeacon[node_index].used = 0;
	}

	bt_distance_detector_flash_store(DETECTOR_FLASH_PARAMS_CLEAR, -1);
}

double bt_distance_detector_calculate_result(uint8_t node_index)
{
	double distance = 0;
	int8_t TxPower = scan_tag_msg.sBeacon[node_index].txpower;
	uint8_t rssi_count = scan_tag_msg.sBeacon[node_index].rssi_count;
	int8_t RssiValue = Average_Filter(node_index, rssi_count);

	distance = pow(10.0, ((TxPower - RssiValue) / (10.0 * DISTANCE_DETECTION_ENVIRONMENTAL_FACTOR)));
	printf("node[%d]: average_rssi(%d) %d, calculate distance %.4f\r\n", node_index, rssi_count, RssiValue, distance);
	//printf("@[%d]\t%4d\t%4d\t%6.4f\r\n", rssi_count-1, scan_tag_msg.sBeacon[node_index].previous_rssi, RssiValue, distance); //only print to get data

	return distance;
}

bool bt_distance_detector_check_msg(device_msg_t dev_info)
{
	int ret = 0;
	uint8_t node_index = 0;
	uint8_t dev_addr[6];
	int8_t rssi = dev_info.dev_rssi;
	int8_t txpower = dev_info.txpower;
	memcpy(dev_addr, dev_info.dev_addr, 6);

	ret = bt_distance_detector_scan_result_check(dev_addr, rssi, txpower);
	if(ret >= 0)
	{
		node_index = (uint8_t)ret;
		bt_distance_detector_calculate_result(node_index);

		return true;
	}

	return false;
}

void bt_distance_detector_beacon_num_check(void)
{
	for (uint8_t node_index = 0; node_index < scan_tag_msg.node_num; node_index++)
	{
		if (scan_tag_msg.sBeacon[node_index].used == 1)
		{
			if (scan_tag_msg.sBeacon[node_index].beacon_num != 0)
			{
				scan_tag_msg.sBeacon[node_index].beacon_num = 0;
			}
			else
			{
				scan_tag_msg.sBeacon[node_index].used = 0;
				bt_distance_detector_flash_store(DETECTOR_FLASH_PARAMS_CLEAR, node_index);
			}
		}
	}
}
#endif
