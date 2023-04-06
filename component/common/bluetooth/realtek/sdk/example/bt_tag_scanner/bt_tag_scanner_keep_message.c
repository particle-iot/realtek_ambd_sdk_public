#include <platform_opts_bt.h>
#if defined(CONFIG_BT_TAG_SCANNER) && CONFIG_BT_TAG_SCANNER
#include <stdio.h>
#include <bt_tag_scanner_keep_message.h>
#include <app_msg.h>
#include <ftl_app.h>
#include <trace_app.h>
#include <string.h>
#include <os_mem.h>
#include <gap_conn_le.h>
#include <gap_storage_le.h>


#define bt_tag_scanner_flash_save(pdata, size, offset)   ftl_save(pdata, offset, size)
#define bt_tag_scanner_flash_load(pdata, size, offset)   ftl_load(pdata, offset, size)
#define BT_TAG_SCANNER_MEMBER_OFFSET(struct_type, member)     ((uint32_t)&((struct_type *)0)->member)

#define FLASH_RPL   1

typedef enum
{
    SCANNER_FLASH_PARAMS_FEATURES,
    SCANNER_FLASH_PARAMS_RPL,
    SCANNER_FLASH_PARAMS_RPL_ENTRY,
    SCANNER_FLASH_PARAMS_RPL_SEQ,
} flash_params_type_t;


typedef struct
{
    uint16_t used: 1;
    uint16_t rfu: 15;
    rpl_mac_t tag_mac;
    uint32_t seq;

}rpl_entry_t;

typedef struct
{
    uint16_t entry_num; //!< the number of replay protection list entries
    uint16_t padding;	//!< 2 bytes padding for alignment
    rpl_entry_t *rpl; 
} scanner_rpl_t;

scanner_rpl_t scanner_rpl;

typedef struct
{
    uint16_t entry_loop;
} rpl_entry_info_t;

typedef struct
{
    uint8_t rpl_loop;
    uint8_t padding[3];
    rpl_entry_t entry[BT_TAG_SCANNER_RPL_ENTRY_NUM_MAX * 2];
}flash_rpl_t;

typedef struct
{
    flash_rpl_t flash_rpl;
}flash_node_t, *flash_node_p;

typedef struct
{
    uint16_t flash_rpl_offset;
    uint16_t flash_scan_tag_info_offset;
    uint16_t flash_total_size;

} flash_layout_info_t, *flash_layout_info_p;

flash_layout_info_t flash_layout_info;

void bt_tag_scanner_flash_init(void)
{ 
    flash_layout_info.flash_rpl_offset = 0;
    flash_layout_info.flash_scan_tag_info_offset = flash_layout_info.flash_rpl_offset + 4 + \
                                        scanner_rpl.entry_num * sizeof(rpl_entry_t);
}

void bt_tag_scanner_flash_store(flash_params_type_t param_type, void *param)
{
    uint32_t ret = 0;

    switch (param_type)
    {
    case SCANNER_FLASH_PARAMS_RPL:
        {
            if (FLASH_RPL)
            {
                rpl_entry_t rpl_entry;
                for (uint16_t loop = 0; loop < scanner_rpl.entry_num; loop++)
                {
                    uint16_t offset = flash_layout_info.flash_rpl_offset + 4 + loop * sizeof(rpl_entry_t);
                    ret = bt_tag_scanner_flash_load((void *)&rpl_entry, 4, offset);
                    if (ret == 0 && rpl_entry.used)
                    {
                        rpl_entry.used = 0;
                        ret = bt_tag_scanner_flash_save((void *)&rpl_entry, 4, offset);
                        if (ret != 0)
                        {
                            goto end;
                        }
                    }
                }
            }
        }
        break;

    case SCANNER_FLASH_PARAMS_RPL_ENTRY:
        {
            if (FLASH_RPL)
            {
                APP_PRINT_INFO0("save new entry(new device address and seq) to flash");

                rpl_entry_info_t *prpl_entry = (rpl_entry_info_t *)param;
                uint16_t offset = flash_layout_info.flash_rpl_offset + 4 + prpl_entry->entry_loop * sizeof(rpl_entry_t);
                ret = bt_tag_scanner_flash_save((void *)&scanner_rpl.rpl[prpl_entry->entry_loop], sizeof(rpl_entry_t), offset);
            }
        }
        break;

    case SCANNER_FLASH_PARAMS_RPL_SEQ:
        {
            if (FLASH_RPL)
            {
                APP_PRINT_INFO0("update seq in flash");

                rpl_entry_info_t *prpl_entry = (rpl_entry_info_t *)param;
                uint16_t offset = flash_layout_info.flash_rpl_offset + 4 + prpl_entry->entry_loop * sizeof(rpl_entry_t) + BT_TAG_SCANNER_MEMBER_OFFSET(rpl_entry_t, seq);
                ret = bt_tag_scanner_flash_save((void *)&scanner_rpl.rpl[prpl_entry->entry_loop].seq, 4, offset);
            }
        }
        break;

    default:
        break;
    }

end:
    if (ret != 0)
    {
        APP_PRINT_INFO2("bt_tag_scanner_flash_store: failed to store the related information, params type = %d, cause = %d",
               param_type, ret);
    }
}

void bt_tag_scanner_flash_restore(void)
{
    uint8_t ret;
    if (FLASH_RPL)
    {
        for (uint16_t entry_loop = 0; entry_loop < scanner_rpl.entry_num; entry_loop++)
        {
            ret = bt_tag_scanner_flash_load((void *)&scanner_rpl.rpl[entry_loop], sizeof(rpl_entry_t),
                                  flash_layout_info.flash_rpl_offset + 4 + entry_loop * sizeof(rpl_entry_t));
            if (ret != 0)
            {
                scanner_rpl.rpl[entry_loop].used = 0;
            }
            else
            {
                if (scanner_rpl.rpl[entry_loop].used == 1)
                {
                    APP_PRINT_INFO3("rpl_ftl_load scanner_rpl.rpl_mac =%s,seq=0x%04x,entry_loop=%d",
                    TRACE_BDADDR(scanner_rpl.rpl[entry_loop].tag_mac.addr), scanner_rpl.rpl[entry_loop].seq, entry_loop);

                    uint8_t *tag_addr = scanner_rpl.rpl[entry_loop].tag_mac.addr;
                    printf("rpl_ftl_load scanner_rpl.rpl_mac = 0x%02x:%02x:%02x:%02x:%02x:%02x, seq=0x%08x, entry_loop=%d\r\n", 
                                    tag_addr[5], tag_addr[4], tag_addr[3], tag_addr[2], tag_addr[1], tag_addr[0],
                                    scanner_rpl.rpl[entry_loop].seq, entry_loop);
                }
            }
        }
    }
}

void rpl_init(uint16_t entry_num)
{
    scanner_rpl.entry_num = entry_num;

    if (entry_num == 0)
    {
        return;
    }

    scanner_rpl.rpl = os_mem_alloc(RAM_TYPE_DATA_ON, sizeof(rpl_entry_t) * entry_num);
    if (scanner_rpl.rpl == NULL)
    {
        APP_PRINT_INFO1("rpl_init: fail to allocate memory, entry num = %d", entry_num);
        return;
    }
    else
    {
        for (uint16_t entry_loop = 0; entry_loop < entry_num; entry_loop++)
        {
            scanner_rpl.rpl[entry_loop].used = 0;
        }
    }
}

void rpl_deinit(void)
{
    if (scanner_rpl.rpl)
    {
        os_mem_free(scanner_rpl.rpl);
        scanner_rpl.rpl = NULL;
    }
}

/*
 * @retval >=0: success
 * @retval -1: fail
 * @retval -2: no space
 */
int rpl_check(rpl_mac_t scan_mac, uint32_t seq, bool update)
{
    if (scanner_rpl.entry_num == 0)
    {
        APP_PRINT_INFO1("scanner_rpl.entry_num = %d", scanner_rpl.entry_num);
        return -2;
    }

    rpl_entry_t *rpl = scanner_rpl.rpl;
    uint16_t loop;
    for (loop = 0; loop < scanner_rpl.entry_num; loop++)
    {
        if ((rpl[loop].used) && (memcmp(rpl[loop].tag_mac.addr, &scan_mac.addr, 6) == 0))
        {
            APP_PRINT_INFO1("rpl bd_addr %s", TRACE_BDADDR(scan_mac.addr));
            if (rpl[loop].seq < seq)
            {
                APP_PRINT_INFO3("rpl_check1:  seq = 0x%06x : 0x%06x, loop = %d", rpl[loop].seq, seq, loop);
                printf("\r\nscanner_rpl.rpl[%d] scan_addr: 0x%02x:%02x:%02x:%02x:%02x:%02x\r\n", loop,
                            scan_mac.addr[5], scan_mac.addr[4], scan_mac.addr[3], 
                            scan_mac.addr[2], scan_mac.addr[1], scan_mac.addr[0]);
                printf("rpl_check1:  seq = 0x%06x : 0x%06x, loop = %d\r\n", rpl[loop].seq, seq, loop);

                if (update)
                {
                    rpl[loop].seq = seq;

                    rpl_entry_info_t rpl_entry;
                    rpl_entry.entry_loop = loop;
                    bt_tag_scanner_flash_store(SCANNER_FLASH_PARAMS_RPL_SEQ, &rpl_entry);
                }
                return loop;
            }
            else
            {
                APP_PRINT_INFO3("rpl_check:  seq = 0x%06x : 0x%06x, loop = %d", rpl[loop].seq, seq, loop);

                return -1;
            }
        }
    }

    /** allocate a new entry to the new device  */
    for (loop = 0; loop < scanner_rpl.entry_num; loop++)
    {
        if (rpl[loop].used == 0)
        {
            APP_PRINT_INFO0("check as a new device");

            if (update)
            {
                APP_PRINT_INFO2("rpl_save rpl[loop] loop =%d:  seq = 0x%06x", loop, seq);
                printf("\r\nrpl_check2: rpl_save scanner_rpl.rpl[%d]:  seq = 0x%06x\r\n", loop, seq);

                rpl[loop].used = 1;
                memcpy(rpl[loop].tag_mac.addr, &scan_mac.addr, 6);
                rpl[loop].seq = seq;

                rpl_entry_info_t rpl_entry;
                rpl_entry.entry_loop = loop;
                bt_tag_scanner_flash_store(SCANNER_FLASH_PARAMS_RPL_ENTRY, &rpl_entry);
            }

            return loop;
        }
    }
    APP_PRINT_INFO0("no space");

    return -2;
}

void rpl_clear(void)
{
    if (scanner_rpl.entry_num == 0)
    {
        return;
    }

    /** clear all the records */
    for (uint16_t loop = 0; loop < scanner_rpl.entry_num; loop++)
    {
        scanner_rpl.rpl[loop].used = 0;
    }

    bt_tag_scanner_flash_store(SCANNER_FLASH_PARAMS_RPL, NULL);
}

void rpl_reset(void)
{
    if (scanner_rpl.entry_num == 0)
    {
        return;
    }

    /** clear all the records */
    for (uint16_t loop = 0; loop < scanner_rpl.entry_num; loop++)
    {
        scanner_rpl.rpl[loop].used = 0;
    }
}

#endif
