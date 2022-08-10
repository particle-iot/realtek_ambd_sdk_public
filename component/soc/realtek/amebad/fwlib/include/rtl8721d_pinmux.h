/**
  ******************************************************************************
  * @file    rtl8721d_pinmux.h
  * @author
  * @version V1.0.0
  * @date    2016-05-17
  * @brief   This file contains all the functions prototypes for the pinmux firmware
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

#ifndef _HAL_8721D_PINMUX_
#define _HAL_8721D_PINMUX_

#include "rtl8721d_pinmux_defines.h"

/** @defgroup PINMUX_Exported_Functions PINMUX Exported Functions
  * @{
  */
_LONG_CALL_ void PAD_CMD(u8 PinName, u8 NewStatus);
_LONG_CALL_ void PAD_DrvStrength(u8 PinName, u32 DrvStrength);
_LONG_CALL_ void PAD_PullCtrl(u8 PinName, u8 PullType);
_LONG_CALL_ void Pinmux_Config(u8 PinName, u32 PinFunc);
_LONG_CALL_ u32 Pinmux_ConfigGet(u8 PinName);
_LONG_CALL_ void Pinmux_UartLogCtrl(u32  PinLocation, BOOL   Operation);
_LONG_CALL_ void Pinmux_SpicCtrl(u32  PinLocation, BOOL Operation);
/**
  *  @brief Set the Internal pad Resistor type.
  *  @param PinName : value of @ref PINMUX_Pin_Name_definitions.
  *  @param PullType : the pull type for the pin.This parameter can be one of the following values:
  *		 @arg GPIO_Resistor_LARGE
  *		 @arg GPIO_Resistor_SMALL
  *  @retval None
  *  @note Just for PAD C/F/G:
  *  @note PA[12]/PA[13]/PA[14]/PA[15]/PA[16]/PA[17]/PA[18]/PA[19]/PA[20]/PA[21]/PA[25]/PA[26] 4.7K/50K
  *  @note PA[29]/PA[30]/PA[31] 4.7K/10K
  */
static inline void PAD_ResistorCtrl(u8 PinName, u8 RType)
{
	u32 Temp = 0;

	/* get PADCTR */
	Temp = PINMUX->PADCTR[PinName];

	/* set resistor small */
	Temp |= PAD_BIT_PULL_RESISTOR_SMALL; /* by default is small */

	/* set large if needed */
	if (RType == GPIO_Resistor_LARGE) {
		Temp &= ~PAD_BIT_PULL_RESISTOR_SMALL;
	}

	/* set PADCTR register */
	PINMUX->PADCTR[PinName] = Temp;
}


/**
  *  @brief Turn off pinmux SWD function.
  *  @retval None
  */
static inline void Pinmux_Swdoff(void)
{
	u32 Temp = 0;
	 
	Temp = HAL_READ32(SYSTEM_CTRL_BASE_LP, REG_SWD_PMUX_EN);	
	Temp &= (~BIT_LSYS_SWD_PMUX_EN);	
	HAL_WRITE32(SYSTEM_CTRL_BASE_LP, REG_SWD_PMUX_EN, Temp);
}


/**
  * @}
  */

/**
  * @}
  */

/**
  * @}
  */

/* Other definations --------------------------------------------------------*/
#define FLASH_S0_CS_GPIO		_PB_18
#define FLASH_S1_CS_GPIO		_PB_16



#endif   //_HAL_8721D_PINMUX_
/******************* (C) COPYRIGHT 2016 Realtek Semiconductor *****END OF FILE****/

