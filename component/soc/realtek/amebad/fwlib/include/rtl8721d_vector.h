/**
  ******************************************************************************
  * @file    rtl8721d_vector.h
  * @author
  * @version V1.0.0
  * @date    2016-05-17
  * @brief   This file contains all the functions prototypes for the IRQ firmware
  *          library.
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2016, Realtek Semiconductor Corporation. All rights reserved.
  ****************************************************************************** 
  */

#ifndef _RTL8710B_VECTOR_TABLE_H_
#define _RTL8710B_VECTOR_TABLE_H_

#include "rtl8721d_vector_defines.h"

/* Exported functions --------------------------------------------------------*/
/** @defgroup IRQ_Exported_Functions IRQ Exported Functions
  * @{
  */
extern _LONG_CALL_ void irq_table_init(u32 StackP);
extern _LONG_CALL_ BOOL irq_register(IRQ_FUN IrqFun, IRQn_Type IrqNum, u32 Data, u32 Priority);
extern _LONG_CALL_ BOOL irq_unregister(IRQn_Type IrqNum);
extern _LONG_CALL_ void irq_enable(IRQn_Type   IrqNum);
extern _LONG_CALL_ void irq_disable(IRQn_Type   IrqNum);

#define InterruptRegister			irq_register_check
#define InterruptUnRegister		irq_unregister

#define InterruptEn(a,b)			irq_enable(a)
#define InterruptDis(a)			irq_disable(a)
/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/* Other Definitions --------------------------------------------------------*/
extern IRQ_FUN UserIrqFunTable[];
extern u32 UserIrqDataTable[];
extern HAL_VECTOR_FUN  NewVectorTable[];

#if defined (ARM_CORE_CM4)
#define MAX_VECTOR_TABLE_NUM			(64+16)
#define MAX_PERIPHERAL_IRQ_NUM		64 
#define MAX_IRQ_PRIORITY_VALUE			7
#define IRQ_PRIORITY_SHIFT				1
#else
#define MAX_VECTOR_TABLE_NUM			(16+32)
#define MAX_PERIPHERAL_IRQ_NUM		32 
#define MAX_IRQ_PRIORITY_VALUE			3
#define IRQ_PRIORITY_SHIFT				2
#endif

#define MSP_RAM_LP			0x0008FFFC
#define VCT_RAM_LP			0x00080000
#define MSP_RAM_HP			0x1007EFFC
#define MSP_RAM_HP_NS		0x10004FFC

static inline BOOL irq_register_check(IRQ_FUN IrqFun, IRQn_Type IrqNum, u32 Data,  u32 Priority) {
	if(Priority > MAX_IRQ_PRIORITY_VALUE) {
		Priority = MAX_IRQ_PRIORITY_VALUE;
	}
	Priority = (Priority << IRQ_PRIORITY_SHIFT);
	return irq_register(IrqFun, IrqNum, Data, Priority);
}
#endif //_RTL8710B_VECTOR_TABLE_H_
/******************* (C) COPYRIGHT 2016 Realtek Semiconductor *****END OF FILE****/
