/*
 * Copyright (C) 2015-2018 Alibaba Group Holding Limited
 */

#include <platform_opts_bt.h>
#if defined(CONFIG_BT_BREEZE) && CONFIG_BT_BREEZE
#include <breeze_hal_os.h>
#include "osdep_service.h"
#include "sys_api.h"
#include "dct.h"

#define BREEZE_DCT_BEGIN_ADDR           0x100000    /*!< DCT begin address of flash, ex: 0x100000 = 1M */
#define BREEZE_MODULE_NUM               6           /*!< max number of module */
#define BREEZE_VARIABLE_NAME_SIZE       128         /*!< max size of the variable name */
#define BREEZE_VARIABLE_VALUE_SIZE_L    4           /*!< size of the variable value length */
#define BREEZE_VARIABLE_VALUE_SIZE_V    256         /*!< max size of the variable value */
#define BREEZE_VARIABLE_VALUE_SIZE      (BREEZE_VARIABLE_VALUE_SIZE_L + BREEZE_VARIABLE_VALUE_SIZE_V)
#define BREEZE_DCT_MODULE_NAME          "breeze_dct_module"

dct_handle_t breeze_dct_handle;
bool breeze_dct_init_flag = 0;

typedef struct {
    _timerHandle timer_hdl;
    unsigned long data;
    void (*function)(void*, void*);
} _breeze_timer;

typedef struct {
    struct list_head list;
    _breeze_timer *timer;
} _breeze_timer_entry;

_list breeze_timer_table;
bool breeze_timer_init_flag = 0;

typedef struct {
    _breeze_timer *ptimer;
    int timeout;
} breeze_timer_t;

void breeze_init_timer_wrapper(void)
{
    if (!breeze_timer_init_flag) {
        rtw_init_listhead(&breeze_timer_table);
        breeze_timer_init_flag = 1;
    }
}

void breeze_deinit_timer_wrapper(void)
{
    if (breeze_timer_init_flag) {
        _list *plist;
        _breeze_timer_entry *timer_entry;

        save_and_cli();

        while (rtw_end_of_queue_search(&breeze_timer_table, get_next(&breeze_timer_table)) == _FALSE) {
            plist = get_next(&breeze_timer_table);
            timer_entry = LIST_CONTAINOR(plist, _breeze_timer_entry, list);

            rtw_list_delete(plist);
            rtw_mfree((u8 *)timer_entry, sizeof(_breeze_timer_entry));
        }

        restore_flags();

        breeze_timer_init_flag = 0;
    }
}

void breeze_timer_wrapper(_timerHandle timer_hdl)
{
    _list *plist;
    _breeze_timer_entry *timer_entry = NULL;

    save_and_cli();

    plist = get_next(&breeze_timer_table);
    while ((rtw_end_of_queue_search(&breeze_timer_table, plist)) == _FALSE)
    {
        timer_entry = LIST_CONTAINOR(plist, _breeze_timer_entry, list);
        if (timer_entry->timer->timer_hdl == timer_hdl) {
            break;
        }
        plist = get_next(plist);
    }

    restore_flags();

    if (plist == &breeze_timer_table)
        printf("Fail to find the timer_entry in timer table!\r\n");
    else {
        if (timer_entry->timer->function)
            timer_entry->timer->function((void *)timer_entry->timer->timer_hdl, (void *)timer_entry->timer->data);
    }
}

int breeze_init_timer(_breeze_timer *timer)
{
    _breeze_timer_entry *timer_entry;

    if (timer->function == NULL)
        return -1;

    if (timer->timer_hdl == NULL) {
        timer->timer_hdl = rtw_timerCreate((signed const char *)"Breeze_Timer", TIMER_MAX_DELAY, _FALSE, NULL, breeze_timer_wrapper);

        if (timer->timer_hdl == NULL) {
            printf("breeze_init_timer: Fail to init timer!\r\n");
            return -1;
        } else {
            timer_entry = (_breeze_timer_entry *)rtw_zmalloc(sizeof(_breeze_timer_entry));

            if(timer_entry == NULL) {
                printf("breeze_init_timer: Fail to alloc timer_entry!\r\n");
                rtw_timerDelete(timer->timer_hdl, TIMER_MAX_DELAY);
                timer->timer_hdl = NULL;
                return -1;
            }

            timer_entry->timer = timer;

            save_and_cli();
            rtw_list_insert_head(&timer_entry->list, &breeze_timer_table);
            restore_flags();
        }
    } else if (rtw_timerIsTimerActive(timer->timer_hdl) == _TRUE) {
        rtw_timerStop(timer->timer_hdl, TIMER_MAX_DELAY);
    }

    return 0;
}

int breeze_mod_timer(_breeze_timer *timer, u32 delay_time_ms)
{
    _breeze_timer_entry *timer_entry;

    if (timer->timer_hdl == NULL) {
        timer->timer_hdl = rtw_timerCreate((signed const char *)"Breeze_Timer", TIMER_MAX_DELAY, _FALSE, NULL, breeze_timer_wrapper);

        if (timer->timer_hdl == NULL) {
            printf("breeze_mod_timer: Fail to init timer!\r\n");
            return -1;
        } else {
            timer_entry = (_breeze_timer_entry *)rtw_zmalloc(sizeof(_breeze_timer_entry));

            if (timer_entry == NULL) {
                printf("breeze_mod_timer: Fail to alloc timer_entry!\r\n");
                rtw_timerDelete(timer->timer_hdl, TIMER_MAX_DELAY);
                timer->timer_hdl = NULL;
                return -1;
            }

            timer_entry->timer = timer;

            save_and_cli();
            rtw_list_insert_head(&timer_entry->list, &breeze_timer_table);
            restore_flags();
        }
    } else if (rtw_timerIsTimerActive(timer->timer_hdl) == _TRUE) {
        rtw_timerStop(timer->timer_hdl, TIMER_MAX_DELAY);
    }

    if (timer->timer_hdl != NULL) {
        if (rtw_timerChangePeriod(timer->timer_hdl, rtw_ms_to_systime(delay_time_ms), TIMER_MAX_DELAY) == _FAIL) {
            printf("breeze_mod_timer: Fail to set timer period!\r\n");
            return -1;
        }
    }

    return 0;
}

int breeze_cancel_timer_ex(_breeze_timer *timer)
{
    _list *plist;
    _breeze_timer_entry *timer_entry = NULL;

    if(timer->timer_hdl == NULL)
        return -1;

    save_and_cli();

    plist = get_next(&breeze_timer_table);
    while ((rtw_end_of_queue_search(&breeze_timer_table, plist)) == _FALSE)
    {
        timer_entry = LIST_CONTAINOR(plist, _breeze_timer_entry, list);
        if (timer_entry->timer->timer_hdl == timer->timer_hdl) {
            break;
        }
        plist = get_next(plist);
    }

    restore_flags();

    if (plist == &breeze_timer_table) {
        printf("breeze_cancel_timer_ex: Fail to find the timer_entry in timer table!\r\n");
        return -1;
    } else {
        rtw_timerStop(timer->timer_hdl, TIMER_MAX_DELAY);
    }

    return 0;
}

int breeze_del_timer_sync(_breeze_timer *timer)
{
    _list *plist;
    _breeze_timer_entry *timer_entry;

    if(timer->timer_hdl == NULL)
        return -1;

    save_and_cli();

    plist = get_next(&breeze_timer_table);
    while ((rtw_end_of_queue_search(&breeze_timer_table, plist)) == _FALSE)
    {
        timer_entry = LIST_CONTAINOR(plist, _breeze_timer_entry, list);
        if (timer_entry->timer->timer_hdl == timer->timer_hdl) {
            rtw_list_delete(plist);
            rtw_mfree((u8 *)timer_entry, sizeof(_breeze_timer_entry));
            break;
        }
        plist = get_next(plist);
    }

    restore_flags();

    if (plist == &breeze_timer_table) {
        printf("breeze_del_timer_sync: Fail to find the timer_entry in timer table!\r\n");
        return -1;
    } else {
        rtw_timerDelete(timer->timer_hdl, TIMER_MAX_DELAY);
        timer->timer_hdl = NULL;
    }

    return 0;
}

int os_timer_new(os_timer_t *timer, os_timer_cb_t cb, void *arg, int ms)
{
    if (!breeze_timer_init_flag) {
        printf("os_timer_new: breeze_timer_wrapper not init!\r\n");
        return -1;
    }

    breeze_timer_t *p_breeze_timer = (breeze_timer_t *)timer->hdl;
    if (p_breeze_timer != NULL) {
        printf("os_timer_new: p_breeze_timer has exist!\r\n");
        return -1;
    }

    p_breeze_timer = (breeze_timer_t *)rtw_zmalloc(sizeof(breeze_timer_t));
    if (p_breeze_timer == NULL) {
        printf("os_timer_new: malloc p_breeze_timer fail!\r\n");
        return -1;
    }

    _breeze_timer *ptimer = (_breeze_timer *)rtw_zmalloc(sizeof(_breeze_timer));
    if (ptimer == NULL) {
        printf("os_timer_new: malloc ptimer fail!\r\n");
        if (p_breeze_timer != NULL) {
            rtw_free(p_breeze_timer);
            p_breeze_timer = NULL;
        }
        return -1;
    }

    ptimer->function = cb;
    ptimer->data = (u32)arg;
    if (breeze_init_timer(ptimer) != 0) {
        printf("os_timer_new: timer create fail!\r\n");
        if (ptimer != NULL) {
            rtw_free(ptimer);
            ptimer = NULL;
        }
        if (p_breeze_timer != NULL) {
            rtw_free(p_breeze_timer);
            p_breeze_timer = NULL;
        }
        return -1;
    }

    p_breeze_timer->ptimer = ptimer;
    p_breeze_timer->timeout = ms;
    timer->hdl = (void *)p_breeze_timer;

    return 0;
}

int os_timer_start(os_timer_t *timer)
{
    breeze_timer_t *p_breeze_timer = (breeze_timer_t *)timer->hdl;
    if (p_breeze_timer == NULL) {
        printf("os_timer_start: p_breeze_timer not exist!\r\n");
        return -1;
    }

    _breeze_timer *ptimer = p_breeze_timer->ptimer;
    if (ptimer == NULL) {
        printf("os_timer_start: ptimer not exist!\r\n");
        return -1;
    }

    u32 delay_time_ms = (u32)p_breeze_timer->timeout;
    if (breeze_mod_timer(ptimer, delay_time_ms) != 0) {
        printf("os_timer_start: timer start fail!\r\n");
        return -1;
    }

    return 0;
}

int os_timer_stop(os_timer_t *timer)
{
    breeze_timer_t *p_breeze_timer = (breeze_timer_t *)timer->hdl;
    if (p_breeze_timer == NULL) {
        printf("os_timer_stop: p_breeze_timer not exist!\r\n");
        return -1;
    }

    _breeze_timer *ptimer = p_breeze_timer->ptimer;
    if (ptimer == NULL) {
        printf("os_timer_stop: ptimer not exist!\r\n");
        return -1;
    }

    if (breeze_cancel_timer_ex(ptimer) != 0) {
        printf("os_timer_stop: timer stop fail!\r\n");
        return -1;
    }

    return 0;
}

void os_timer_free(os_timer_t *timer)
{
    breeze_timer_t *p_breeze_timer = (breeze_timer_t *)timer->hdl;
    if (p_breeze_timer == NULL) {
        printf("os_timer_free: p_breeze_timer not exist!\r\n");
        return;
    }

    _breeze_timer *ptimer = p_breeze_timer->ptimer;
    if (ptimer == NULL) {
        printf("os_timer_free: ptimer not exist!\r\n");
        return;
    }

    if (breeze_del_timer_sync(ptimer) != 0) {
        printf("os_timer_free: timer delete fail!\r\n");
    } else {
        if (ptimer != NULL) {
            rtw_free(ptimer);
            ptimer = NULL;
        }
        if (p_breeze_timer != NULL) {
            rtw_free(p_breeze_timer);
            p_breeze_timer = NULL;
        }
        timer->hdl = NULL;
    }
}

void os_reboot(void)
{
    sys_reset();
}

void os_msleep(int ms)
{
    rtw_msleep_os(ms);
}

long long os_now_ms(void)
{
    long long current_time;
    current_time = (long long)rtw_get_current_time();
    return current_time;
}

void breeze_init_dct(void)
{
    if (!breeze_dct_init_flag) {
        dct_init(BREEZE_DCT_BEGIN_ADDR, BREEZE_MODULE_NUM, BREEZE_VARIABLE_NAME_SIZE, BREEZE_VARIABLE_VALUE_SIZE, 1, 0);
        dct_register_module(BREEZE_DCT_MODULE_NAME);
        dct_open_module(&breeze_dct_handle, BREEZE_DCT_MODULE_NAME);
        breeze_dct_init_flag = 1;
    }
}

void breeze_deinit_dct(void)
{
    if (breeze_dct_init_flag) {
        dct_close_module(&breeze_dct_handle);
        dct_unregister_module(BREEZE_DCT_MODULE_NAME);
        dct_deinit();
        breeze_dct_init_flag = 0;
    }
}

int os_kv_set(const char *key, const void *value, int len, int sync)
{
    return dct_set_variable_new(&breeze_dct_handle, (char *)key, (char *)value, (uint16_t)len);
}

int os_kv_get(const char *key, void *buffer, int *buffer_len)
{
    return dct_get_variable_new(&breeze_dct_handle, (char *)key, (char *)buffer, (uint16_t *)buffer_len);
}

int os_kv_del(const char *key)
{
    return dct_delete_variable_new(&breeze_dct_handle, (char *)key);
}

int os_rand(void)
{
    int random_int;
    u8 random_bytes[4];

    rtw_get_random_bytes(random_bytes, 4);
    random_int = (random_bytes[3] << 24) | (random_bytes[2] << 16) | (random_bytes[1] << 8) | random_bytes[0];

    return random_int;
}

int os_get_version_chip_info(char* info, uint8_t* p_len)
{
    char *chip_info = "AmebaD:Realtek";

    if (info == NULL || p_len == NULL) {
        printf("os_get_version_chip_info: invalid paramters!\r\n");
        return -1;
    }

    rtw_memcpy(info, chip_info, strlen(chip_info));
    *p_len = strlen(chip_info);

    return 0;
}
#endif
