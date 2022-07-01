/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <platform_opts_bt.h>
#if defined(CONFIG_BT_BREEZE) && CONFIG_BT_BREEZE
#include <stdio.h>
#include <string.h>
#include "breeze_hal_ble.h"
#include "bt_breeze_app_task.h"
#include "bt_breeze_peripheral_app.h"
#include "bt_breeze_app_flags.h"
#include "breeze.h"
#include "profile_server.h"

#include <gap_msg.h>
#include <gap.h>
#include <gap_adv.h>
#include <gap_le_types.h>
#include "os_msg.h"
#include <osdep_service.h>

#define AD_LEN 33   // 1+2+1+3+1+1+MAX_VENDOR_DATA_LEN
#define SD_LEN 33

extern T_GAP_DEV_STATE ble_breeze_gap_dev_state;

uint16_t ais_ic_ccc_cfg = GATT_CLIENT_CHAR_CONFIG_DEFAULT;
uint16_t ais_nc_ccc_cfg = GATT_CLIENT_CHAR_CONFIG_DEFAULT;

ais_bt_init_t *bt_init_info = NULL;

static stack_init_done_t stack_init_done;

static T_ATTRIB_APPL *ais_attrs = NULL;

#if BREEZE_TEMP_WORK_AROUND
#define MAX_IND_NOTIFY_PARAM_LIST_SIZE 10
_queue free_ind_notify_param_queue;
void *ind_notify_param_list_buf;
int free_ind_notify_param_list_count = 0;
int max_ind_notify_param_list_size = MAX_IND_NOTIFY_PARAM_LIST_SIZE;

void ble_breeze_init_ind_notify_param(void)
{
    ind_notify_param_list_t *p_ind_notify_param_list;

    rtw_init_queue(&free_ind_notify_param_queue);

    ind_notify_param_list_buf = rtw_zmalloc(sizeof(ind_notify_param_list_t) * max_ind_notify_param_list_size);

    p_ind_notify_param_list = (ind_notify_param_list_t *)ind_notify_param_list_buf;
    for (int i = 0; i < max_ind_notify_param_list_size; i++) {
        rtw_init_listhead(&(p_ind_notify_param_list->list));
        rtw_list_insert_tail(&(p_ind_notify_param_list->list), &(free_ind_notify_param_queue.queue));
        p_ind_notify_param_list++;
    }
    free_ind_notify_param_list_count = max_ind_notify_param_list_size;
}

void ble_breeze_deinit_ind_notify_param(void)
{
    free_ind_notify_param_list_count = 0;
    max_ind_notify_param_list_size = MAX_IND_NOTIFY_PARAM_LIST_SIZE;

    rtw_free(ind_notify_param_list_buf);

    rtw_spinlock_free(&(free_ind_notify_param_queue.lock));
}

ind_notify_param_list_t *ble_breeze_alloc_ind_notify_param(void)
{
    _irqL irqL;
    _list *plist, *phead;
    ind_notify_param_list_t *p_ind_notify_param_list;

    rtw_enter_critical_bh(&(free_ind_notify_param_queue.lock), &irqL);

    if (rtw_queue_empty(&free_ind_notify_param_queue) == _TRUE) {
        p_ind_notify_param_list = NULL;
    } else {
        phead = get_list_head(&free_ind_notify_param_queue);
        plist = get_next(phead);
        p_ind_notify_param_list = LIST_CONTAINOR(plist, ind_notify_param_list_t, list);
        rtw_list_delete(&(p_ind_notify_param_list->list));
    }

    if (p_ind_notify_param_list != NULL) {
        free_ind_notify_param_list_count--;
    }

    rtw_exit_critical_bh(&(free_ind_notify_param_queue.lock), &irqL);

    return p_ind_notify_param_list;
}

void ble_breeze_free_ind_notify_param(ind_notify_param_list_t *p_ind_notify_param_list)
{
    _irqL irqL;

    if (p_ind_notify_param_list == NULL) {
        printf("ble_breeze_free_ind_notify_param: p_ind_notify_param_list is null!\r\n");
        return;
    }

    rtw_enter_critical_bh(&(free_ind_notify_param_queue.lock), &irqL);

    memset(&(p_ind_notify_param_list->ind_notify_param), 0, sizeof(ind_notify_param_t));
    rtw_list_delete(&(p_ind_notify_param_list->list));
    rtw_list_insert_tail(&(p_ind_notify_param_list->list), get_list_head(&free_ind_notify_param_queue));
    free_ind_notify_param_list_count++;

    rtw_exit_critical_bh(&(free_ind_notify_param_queue.lock), &irqL);

    return;
}
#endif

void connected()
{
    if (bt_init_info && (bt_init_info->on_connected)) {
        bt_init_info->on_connected();
    }
}

void disconnected()
{
    if (bt_init_info && (bt_init_info->on_disconnected)) {
        bt_init_info->on_disconnected();
    }
}

void ais_nc_ccc_cfg_changed(uint8_t value)
{
    ais_ccc_value_t val;

    switch (value)
    {
    case BREEZE_NOTIFY_ENABLE:
        {
            printf("CCC cfg changed to NTF (%d)\r\n", value);
            val = AIS_CCC_VALUE_NOTIFY;
        }
        break;

    case BREEZE_NOTIFY_DISABLE:
        {
            printf("CCC cfg changed to NTF (%d)\r\n", value);
            val = AIS_CCC_VALUE_NONE;
        }
        break;
    default:
        return;
    }

    if (bt_init_info && bt_init_info->nc.on_ccc_change) {
        bt_init_info->nc.on_ccc_change(val);
    }
}

void ais_ic_ccc_cfg_changed(uint8_t value)
{
    ais_ccc_value_t val;
    switch (value)
    {
    case BREEZE_INDICATE_ENABLE:
        {
            printf("CCC cfg changed to IND (%d)\r\n", value);
            val = AIS_CCC_VALUE_INDICATE;
        }
        break;
    case BREEZE_INDICATE_DISABLE:
        {
            printf("CCC cfg changed to IND (%d)\r\n", value);
            val = AIS_CCC_VALUE_NONE;
        }
        break;
    default:
        return;
    }

    if (bt_init_info && bt_init_info->ic.on_ccc_change) {
        bt_init_info->ic.on_ccc_change(val);
    }
}

int read_ais_rc(void *buf, uint16_t len)
{
    int ret = 0;

    if (bt_init_info && bt_init_info->rc.on_read) {
        ret = bt_init_info->rc.on_read(buf, len);
    }

    return ret;
}

int read_ais_wc(void *buf, uint16_t len)
{
    int ret = 0;

    if (bt_init_info && bt_init_info->wc.on_read) {
        ret = bt_init_info->wc.on_read(buf, len);
    }

    return ret;
}

int write_ais_wc(void *buf, uint16_t len)
{
    int ret = 0;

    if (bt_init_info && bt_init_info->wc.on_write) {
        ret = bt_init_info->wc.on_write(buf, len);
    }

    return ret;
}

int read_ais_ic(void *buf, uint16_t len)
{
    int ret = 0;

    if (bt_init_info && bt_init_info->ic.on_read) {
        ret = bt_init_info->ic.on_read(buf, len);
    }

    return ret;
}

int read_ais_wwnrc(void *buf, uint16_t len)
{
    int ret = 0;

    if (bt_init_info && bt_init_info->wwnrc.on_read) {
        ret = bt_init_info->wwnrc.on_read(buf, len);
    }

    return ret;
}

int write_ais_wwnrc(void *buf, uint16_t len)
{
    int ret = 0;

    if (bt_init_info && bt_init_info->wwnrc.on_write) {
        ret = bt_init_info->wwnrc.on_write(buf, len);
    }

    return ret;
}

int read_ais_nc(void *buf, uint16_t len)
{
    int ret = 0;

    if (bt_init_info && bt_init_info->nc.on_read) {
        ret = bt_init_info->nc.on_read(buf, len);
    }

    return ret;
}

static ais_service_cb_t ais_cb = {
    read_ais_rc,
    read_ais_wc,
    read_ais_ic,
    read_ais_nc,
    read_ais_wwnrc,
    write_ais_wc,
    write_ais_wwnrc,
    ais_ic_ccc_cfg_changed,
    ais_nc_ccc_cfg_changed,
    connected,
    disconnected
};
ais_service_cb_t *ais_service_cb_info = &ais_cb;

static int setup_ais_char_ccc_attr(T_ATTRIB_APPL *attr, uint16_t cccd)
{
    attr->flags         = ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_CCCD_APPL;
    attr->type_value[0] = LO_WORD(GATT_UUID_CHAR_CLIENT_CONFIG);
    attr->type_value[1] = HI_WORD(GATT_UUID_CHAR_CLIENT_CONFIG);
    attr->type_value[3] = LO_WORD(cccd);
    attr->type_value[4] = HI_WORD(cccd);
    attr->value_len     = 2;
    attr->permissions   = GATT_PERM_READ | GATT_PERM_WRITE;

    return 0;
}

uint8_t switch_perm(uint8_t perm)
{
    uint8_t read_perm = 0, write_perm = 0;

    if(perm&AIS_GATT_PERM_READ)
        read_perm = GATT_PERM_READ;
    if (perm&AIS_GATT_PERM_READ_ENCRYPT)
        read_perm = GATT_PERM_READ_ENCRYPTED_REQ;
    if (perm&AIS_GATT_PERM_READ_AUTHEN)
        read_perm = GATT_PERM_READ_AUTHEN_REQ;

    if (perm&AIS_GATT_PERM_WRITE)
        write_perm = GATT_PERM_WRITE;
    if (perm&AIS_GATT_PERM_WRITE_ENCRYPT)
        write_perm = GATT_PERM_WRITE_ENCRYPTED_REQ;
    if (perm&AIS_GATT_PERM_WRITE_AUTHEN)
        write_perm = GATT_PERM_WRITE_AUTHEN_REQ;

    return (read_perm|write_perm);
}

static int setup_ais_char_desc_attr(T_ATTRIB_APPL *attr, ais_uuid_16_t *uuid, uint8_t perm)
{
    uint16_t char_uuid  = uuid->val;

    attr->flags         = ATTRIB_FLAG_VALUE_APPL;
    attr->type_value[0] = LO_WORD(char_uuid);
    attr->type_value[1] = HI_WORD(char_uuid);
    attr->value_len     = 0;
    attr->permissions   = switch_perm(perm);
    attr->p_value_context = NULL;

    return 0;
}

static int setup_ais_char_attr(T_ATTRIB_APPL *attr, ais_uuid_16_t *uuid, uint8_t prop)
{
    uint16_t char_uuid  = uuid->val;

    attr->flags         = ATTRIB_FLAG_VALUE_INCL;
    attr->type_value[0] = LO_WORD(GATT_UUID_CHARACTERISTIC);
    attr->type_value[1] = HI_WORD(GATT_UUID_CHARACTERISTIC);
    attr->type_value[2] = prop;
    attr->type_value[3] = LO_WORD(char_uuid);
    attr->type_value[4] = HI_WORD(char_uuid);
    attr->value_len     = 3;
    attr->permissions   = GATT_PERM_READ;

    return 0;
}

static int setup_ais_service_attr(T_ATTRIB_APPL *attr, ais_uuid_16_t *uuid)
{
    uint16_t serv_uuid  = uuid->val;

    attr->flags         = ATTRIB_FLAG_VALUE_INCL | ATTRIB_FLAG_LE;
    attr->type_value[0] = LO_WORD(GATT_UUID_PRIMARY_SERVICE);
    attr->type_value[1] = HI_WORD(GATT_UUID_PRIMARY_SERVICE);
    attr->type_value[2] = LO_WORD(serv_uuid);
    attr->type_value[3] = HI_WORD(serv_uuid);
    attr->value_len     = 2;
    attr->permissions   = GATT_PERM_READ;

    return 0;
}

T_ATTRIB_APPL attrs[AIS_ATTR_NUM];
extern void ble_breeze_app_le_profile_init(void);
static void bt_ready(void)
{
    uint32_t attr_cnt = AIS_ATTR_NUM, size;
    ais_char_init_t *c;

    size = attr_cnt * sizeof(T_ATTRIB_APPL);

    ais_attrs = attrs;

    memset(ais_attrs, 0, size);

    /* AIS primary service */
    setup_ais_service_attr(&ais_attrs[SVC_ATTR_IDX], (ais_uuid_16_t *)bt_init_info->uuid_svc);

    /* rc */
    c = &(bt_init_info->rc);
    setup_ais_char_attr(&ais_attrs[RC_CHRC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->prop);
    setup_ais_char_desc_attr(&ais_attrs[RC_DESC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->perm);

    /* wc */
    c = &(bt_init_info->wc);
    setup_ais_char_attr(&ais_attrs[WC_CHRC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->prop);
    setup_ais_char_desc_attr(&ais_attrs[WC_DESC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->perm);


    /* ic */
    c = &(bt_init_info->ic);
    setup_ais_char_attr(&ais_attrs[IC_CHRC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->prop);
    setup_ais_char_desc_attr(&ais_attrs[IC_DESC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->perm);
    setup_ais_char_ccc_attr(&ais_attrs[IC_CCC_ATTR_IDX], ais_ic_ccc_cfg);

    /* wwnrc */
    c = &(bt_init_info->wwnrc);
    setup_ais_char_attr(&ais_attrs[WWNRC_CHRC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->prop);
    setup_ais_char_desc_attr(&ais_attrs[WWNRC_DESC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->perm);

    /* nc */
    c = &(bt_init_info->nc);
    setup_ais_char_attr(&ais_attrs[NC_CHRC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->prop);
    setup_ais_char_desc_attr(&ais_attrs[NC_DESC_ATTR_IDX], (ais_uuid_16_t *)c->uuid, c->perm);
    setup_ais_char_ccc_attr(&ais_attrs[NC_CCC_ATTR_IDX], ais_nc_ccc_cfg);


    ble_breeze_app_le_profile_init();

    stack_init_done(0);
}

extern void breeze_init_dct(void);
extern void breeze_init_timer_wrapper(void);
extern int ble_breeze_app_init(void);
ais_err_t ble_stack_init(ais_bt_init_t *info, stack_init_done_t init_done)
{
    bt_init_info = info;
    stack_init_done = init_done;

    breeze_init_dct();
    breeze_init_timer_wrapper();
#if BREEZE_TEMP_WORK_AROUND
    ble_breeze_init_ind_notify_param();
#endif
    ble_breeze_app_init();

    bt_ready();

    return AIS_ERR_SUCCESS;
}

extern void ble_breeze_app_deinit(void);
extern void breeze_deinit_timer_wrapper(void);
extern void breeze_deinit_dct(void);
ais_err_t ble_stack_deinit()
{
    ble_breeze_app_deinit();

#if BREEZE_TEMP_WORK_AROUND
    ble_breeze_deinit_ind_notify_param();
#endif
    breeze_deinit_timer_wrapper();
    breeze_deinit_dct();

    return AIS_ERR_SUCCESS;
}

extern void *ble_breeze_evt_queue_handle;
extern void *ble_breeze_io_queue_handle;
void ble_breeze_send_msg(BT_BREEZE_MSG_TYPE sub_type, void *arg)
{

    uint8_t event = EVENT_IO_TO_APP;

    T_IO_MSG io_msg;

    io_msg.type = IO_MSG_TYPE_QDECODE;
    io_msg.subtype = sub_type;
    io_msg.u.buf = arg;

    if (ble_breeze_evt_queue_handle != NULL && ble_breeze_io_queue_handle != NULL) {
        if (os_msg_send(ble_breeze_io_queue_handle, &io_msg, 0) == false) {
            printf("ble breeze send msg fail: subtype 0x%x", io_msg.subtype);
        } else if (os_msg_send(ble_breeze_evt_queue_handle, &event, 0) == false) {
            printf("ble breeze send event fail: subtype 0x%x", io_msg.subtype);
        }
    }
}

ais_err_t ble_send_notification(uint8_t *p_data, uint16_t length)
{
#if BREEZE_TEMP_WORK_AROUND
    ind_notify_param_list_t *p_ind_notify_param_list = ble_breeze_alloc_ind_notify_param();
    if (p_ind_notify_param_list == NULL) {
        printf("ble_send_notification: malloc p_ind_notify_param_list fail!\r\n");
        return AIS_ERR_MEM_FAIL;
    }

    p_ind_notify_param_list->ind_notify_param.length = length;
    p_ind_notify_param_list->ind_notify_param.ic.p_data = p_data;

    ble_breeze_send_msg(BT_BREEZE_MSG_NOTIFY, p_ind_notify_param_list);
#else
    ind_notify_param_t *param = (ind_notify_param_t *)rtw_zmalloc(sizeof(ind_notify_param_t));

    param->length = length;
    param->p_data = p_data;

    ble_breeze_send_msg(BT_BREEZE_MSG_NOTIFY, param);
#endif

    return AIS_ERR_SUCCESS;
}

ais_err_t ble_send_indication(uint8_t *p_data, uint16_t length, void (*txdone)(uint8_t res))
{
#if BREEZE_TEMP_WORK_AROUND
    ind_notify_param_list_t *p_ind_notify_param_list = ble_breeze_alloc_ind_notify_param();
    if (p_ind_notify_param_list == NULL) {
        printf("ble_send_indication: malloc p_ind_notify_param_list fail!\r\n");
        return AIS_ERR_MEM_FAIL;
    }

    p_ind_notify_param_list->ind_notify_param.length = length;
    p_ind_notify_param_list->ind_notify_param.ic.p_data = p_data;
    p_ind_notify_param_list->ind_notify_param.ic.cb = txdone;

    ble_breeze_send_msg(BT_BREEZE_MSG_INDICATE, p_ind_notify_param_list);
#else
    ind_notify_param_t *param = (ind_notify_param_t *)rtw_zmalloc(sizeof(ind_notify_param_t));

    param->length = length;
    param->ic.p_data = p_data;
    param->ic.cb = txdone;

    ble_breeze_send_msg(BT_BREEZE_MSG_INDICATE, param);
#endif

    return AIS_ERR_SUCCESS;
}

void ble_disconnect(uint8_t reason)
{
    ble_breeze_send_msg(BT_BREEZE_MSG_DISCONNECT, NULL);
    return;
}

extern void ble_breeze_app_le_gap_adv_init(uint8_t adv_len, uint8_t *adv_data, uint8_t scanrsp_len, uint8_t *scanrsp_data);
ais_err_t ble_breeze_set_adv(ais_adv_init_t *adv)
{
    uint8_t flag = 0, adv_len = AD_LEN, scanrsp_len = SD_LEN;

    static uint8_t adv_data[AD_LEN];
    static uint8_t scanrsp_data[SD_LEN];


    if (adv->flag & AIS_AD_GENERAL) {
        flag |= GAP_ADTYPE_FLAGS_GENERAL;
    }
    if (adv->flag & AIS_AD_NO_BREDR) {
        flag |= GAP_ADTYPE_FLAGS_BREDR_NOT_SUPPORTED;
    }
    if (!flag) {
        printf("Invalid adv flag\r\n");
        return AIS_ERR_INVALID_ADV_DATA;
    }

    adv_data[0] = 0x02;
    adv_data[1] = GAP_ADTYPE_FLAGS;
    adv_data[2] = flag;
    adv_data[3] = 0x03;
    adv_data[4] = GAP_ADTYPE_16BIT_COMPLETE;
    adv_data[5] = LO_WORD(BLE_UUID_AIS_SERVICE);
    adv_data[6] = HI_WORD(BLE_UUID_AIS_SERVICE);


    if (adv->vdata.len != 0) {
        adv_data[7] = adv->vdata.len + 1;
        adv_data[8] = GAP_ADTYPE_MANUFACTURER_SPECIFIC;
        for(uint8_t i = 0;i < adv_len;i++)
            adv_data[9+i] = adv->vdata.data[i];
        adv_len = 8 + adv_data[7];
    }else {
        adv_len = 7;
    }

    scanrsp_data[0] = strlen(adv->name.name) + 1;
    switch (adv->name.ntype)
    {
        case AIS_ADV_NAME_SHORT:
            scanrsp_data[1] = GAP_ADTYPE_LOCAL_NAME_SHORT;
            break;
        case AIS_ADV_NAME_FULL:
            scanrsp_data[1] = GAP_ADTYPE_LOCAL_NAME_COMPLETE;
            break;
        default:
            printf("Invalid adv name type.\r\n");
            return AIS_ERR_INVALID_ADV_DATA;
    }

    if (adv->name.name == NULL) {
        printf("Invalid adv name.\r\n");
        return AIS_ERR_INVALID_ADV_DATA;
    }else {
        scanrsp_len = 1 + scanrsp_data[0];
    }

    for(uint8_t j = 0;j < strlen(adv->name.name);j++)
        scanrsp_data[2+j] = adv->name.name[j];

    ble_breeze_app_le_gap_adv_init(adv_len,adv_data, scanrsp_len, scanrsp_data);

    return AIS_ERR_SUCCESS;
}

ais_err_t ble_advertising_start(ais_adv_init_t *adv)
{
    int err;
    T_GAP_DEV_STATE new_state = {0};

    err = ble_breeze_set_adv(adv);
    if (err) {
        printf("Failed to start adv (err %d)\r\n", err);
        return AIS_ERR_ADV_FAIL;
    }

    ble_breeze_send_msg(BT_BREEZE_MSG_START_ADV, NULL);
    do {
        rtw_mdelay_os(1);
        new_state = ble_breeze_gap_dev_state;
    } while (new_state.gap_adv_state != GAP_ADV_STATE_ADVERTISING);

    return AIS_ERR_SUCCESS;
}


ais_err_t ble_advertising_stop()
{
    T_GAP_DEV_STATE new_state = {0};

    ble_breeze_send_msg(BT_BREEZE_MSG_STOP_ADV, NULL);
    do {
        rtw_mdelay_os(1);
        new_state = ble_breeze_gap_dev_state;
    } while (new_state.gap_adv_state != GAP_ADV_STATE_IDLE);

    return AIS_ERR_SUCCESS;
}

ais_err_t ble_get_mac(uint8_t *mac)
{
    uint8_t bt_addr[6];

    if (gap_get_param(GAP_PARAM_BD_ADDR, bt_addr) != GAP_CAUSE_SUCCESS) {
        printf("Failed to get local addr.\r\n");
        return AIS_ERR_STACK_FAIL;
    }

    memcpy(mac, bt_addr, 6);
    printf("Local addr got (%02x:%02x:%02x:%02x:%02x:%02x).\r\n", mac[0],
               mac[1], mac[2], mac[3], mac[4], mac[5]);

    return AIS_ERR_SUCCESS;
}
#endif
