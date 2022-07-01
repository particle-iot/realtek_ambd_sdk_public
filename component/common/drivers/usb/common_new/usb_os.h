/**
  ******************************************************************************
  * @file    usb_ch9.h
  * @author  Realsil WLAN5 Team
  * @brief   This file provides general defines for USB SPEC CH9
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

#ifndef USB_HAL_H
#define USB_HAL_H

/* Includes ------------------------------------------------------------------*/

#include "ameba_soc.h"
#include "basic_types.h"
#include "osdep_service.h"

/* Exported defines ----------------------------------------------------------*/

#define USB_RTOS_ENABLE	1U

#define USB_UNLOCKED	0U
#define USB_LOCKED		1U

/* Exported types ------------------------------------------------------------*/

#if USB_RTOS_ENABLE
typedef _lock usb_spinlock_t;
#else
typedef u8 usb_spinlock_t;
#endif

/* Exported macros -----------------------------------------------------------*/

#ifndef UNUSED
#define UNUSED(X)			(void)X
#endif

#ifndef USB_DMA_ALIGNED
#define USB_DMA_ALIGNED		ALIGNMTO(32)
#endif

#ifndef USB_LOW_BYTE
#define USB_LOW_BYTE(x)		((u8)(x & 0x00FFU))
#endif

#ifndef USB_HIGH_BYTE
#define USB_HIGH_BYTE(x)	((u8)((x & 0xFF00U) >> 8U))
#endif

#ifndef MIN
#define MIN(a, b)			(((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b)			(((a) > (b)) ? (a) : (b))
#endif

/* Exported variables --------------------------------------------------------*/

/* Exported functions --------------------------------------------------------*/

inline void usb_os_delay_ms(u32 ms)
{
	rtw_msleep_os(ms); // DelayMs(ms);
	//rtw_mdelay_os(ms); // vtaskdelay
}

inline void usb_os_delay_us(u32 us)
{
	rtw_usleep_os(us); // DelayUs(us);
	//rtw_udelay_os(us); // vtaskdelay
}

#if USB_RTOS_ENABLE
inline usb_spinlock_t *usb_os_spinlock_alloc(void)
{
	usb_spinlock_t *lock = NULL;
	lock = (usb_spinlock_t *)rtw_zmalloc(sizeof(usb_spinlock_t));
	if (lock != NULL) {
		rtw_spinlock_init((_lock *)lock);
	}
	return lock;
}

inline void usb_os_spinlock_free(usb_spinlock_t *lock)
{
	if (lock != NULL) {
		rtw_spinlock_free((_lock *)lock);
		rtw_free((void *)lock);
	}
}
#endif

inline void usb_os_spinlock(usb_spinlock_t *lock)
{
#if USB_RTOS_ENABLE
	rtw_spin_lock((_lock *)lock);
#else
	if (*(u8 *)lock != USB_LOCKED) {
		*(u8 *)lock = USB_LOCKED;
	}
#endif
}

inline void usb_os_spinunlock(usb_spinlock_t *lock)
{
#if USB_RTOS_ENABLE
	rtw_spin_unlock((_lock *)lock);
#else
	*(u8 *)lock = USB_UNLOCKED;
#endif
}

inline void usb_os_spinlock_safe(usb_spinlock_t *lock)
{
	USB_DisableInterrupt();
	usb_os_spinlock(lock);
}

inline void usb_os_spinunlock_safe(usb_spinlock_t *lock)
{
	usb_os_spinunlock(lock);
	USB_EnableInterrupt();
}

#endif /* USB_HAL_H */

