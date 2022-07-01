/**
  ******************************************************************************
  * @file    example_usbd_cdc_acm_new.h
  * @author  Realsil WLAN5 Team
  * @version V1.0.0
  * @date    2021-6-18
  * @brief   This file is the header file for example_usbd_cdc_acm_new.c
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef EXAMPLE_USBD_CDC_ACM_NEW_H
#define EXAMPLE_USBD_CDC_ACM_NEW_H

#include <platform_opts.h>

#if defined(CONFIG_EXAMPLE_USBD_CDC_ACM) && CONFIG_EXAMPLE_USBD_CDC_ACM

/* Includes ------------------------------------------------------------------*/

/* Exported types ------------------------------------------------------------*/

/* Exported macros -----------------------------------------------------------*/

/* Exported variables --------------------------------------------------------*/

/* Exported functions ------------------------------------------------------- */

void example_usbd_cdc_acm(void);

#endif

#endif /* EXAMPLE_USBD_CDC_ACM_H */

