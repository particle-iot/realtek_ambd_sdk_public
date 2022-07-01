/**
  ******************************************************************************
  * @file    usbd_msc.c
  * @author  Realsil WLAN5 Team
  * @brief   This file provides the functionalities of the USBD MSC
  ******************************************************************************
  * @attention
  *
  * This module is a confidential and proprietary property of RealTek and
  * possession or use of this module requires written permission of RealTek.
  *
  * Copyright(c) 2021, Realtek Semiconductor Corporation. All rights reserved.
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/

#include "usbd_msc.h"
#include "usbd_pcd.h"
#include "usbd_scsi.h"
#include "usbd_core.h"

/* Private defines -----------------------------------------------------------*/

/* Private types -------------------------------------------------------------*/

/* Private macros ------------------------------------------------------------*/

/* Private function prototypes -----------------------------------------------*/

static u8 usbd_msc_set_config(usb_dev_t *dev, u8 config);
static u8 usbd_msc_clear_config(usb_dev_t *dev, u8 config);
static u8 usbd_msc_setup(usb_dev_t *dev, usb_setup_req_t *req);
static u8 *usbd_msc_get_descriptor(usbd_desc_type_t desc, usbd_speed_type_t speed, u16 *len);
static u8 usbd_msc_handle_ep_data_in(usb_dev_t *dev, u8 ep_num);
static u8 usbd_msc_handle_ep_data_out(usb_dev_t *dev, u8 ep_num, u16 len);

/* Private variables ---------------------------------------------------------*/

/* USB Standard Device Descriptor */
static u8 usbd_msc_dev_desc[USB_LEN_DEV_DESC] USB_DMA_ALIGNED = {
	USB_LEN_DEV_DESC,                               /* bLength */
	USB_DESC_TYPE_DEVICE,                           /* bDescriptorType */
	0x00,                                           /* bcdUSB */
	0x02,
	0x00,                                           /* bDeviceClass */
	0x00,                                           /* bDeviceSubClass */
	0x00,                                           /* bDeviceProtocol */
	USB_MAX_EP0_SIZE,                               /* bMaxPacketSize */
	USB_LOW_BYTE(USBD_MSC_VID),                      /* idVendor */
	USB_HIGH_BYTE(USBD_MSC_VID),
	USB_LOW_BYTE(USBD_MSC_PID),                      /* idProduct */
	USB_HIGH_BYTE(USBD_MSC_PID),
	0x00,                                           /* bcdDevice */
	0x02,
	USBD_IDX_MFC_STR,                               /* iManufacturer */
	USBD_IDX_PRODUCT_STR,                           /* iProduct */
	USBD_IDX_SERIAL_STR,                            /* iSerialNumber */
	USBD_MAX_NUM_CONFIGURATION                      /* bNumConfigurations */
};  /* usbd_msc_dev_desc */

/* USB Standard String Descriptor 0 */
static u8 usbd_msc_lang_id_desc[USB_LEN_LANGID_STR_DESC] USB_DMA_ALIGNED = {
	USB_LEN_LANGID_STR_DESC,                        /* bLength */
	USB_DESC_TYPE_STRING,                           /* bDescriptorType */
	USB_LOW_BYTE(USBD_MSC_LANGID_STRING),            /* wLANGID */
	USB_HIGH_BYTE(USBD_MSC_LANGID_STRING),
};  /* usbd_msc_lang_id_desc */

/* USB Standard Manufacture String Descriptor */
static u8 usbd_msc_mfg_string_desc[USBD_MSC_MFG_STRING_DESC_SIZE] USB_DMA_ALIGNED = {
	USBD_MSC_MFG_STRING_DESC_SIZE,                   /* bLength */
	USB_DESC_TYPE_STRING,                           /* bDescriptorType */
	'R',                                            /* bString */
	'e',
	'a',
	'l',
	't',
	'e',
	'k'
};  /* usbd_msc_mfg_string_desc */

/* USB Standard Product String Descriptor for high-speed mode */
static u8 usbd_msc_hs_product_string_desc[USBD_MSC_PRODUCT_STRING_DESC_SIZE] USB_DMA_ALIGNED = {
	USBD_MSC_PRODUCT_STRING_DESC_SIZE,               /* bLength */
	USB_DESC_TYPE_STRING,                           /* bDescriptorType */
	'R',                                            /* bString */
	'e',
	'a',
	'l',
	't',
	'e',
	'k',
	' ',
	'V',
	'C',
	'P',
	' ',
	'(',
	'H',
	'S',
	')',
};  /* usbd_msc_hs_product_string_desc */

/* USB Standard Product String Descriptor for high-speed mode */
static u8 usbd_msc_fs_product_string_desc[USBD_MSC_PRODUCT_STRING_DESC_SIZE] USB_DMA_ALIGNED = {
	USBD_MSC_PRODUCT_STRING_DESC_SIZE,               /* bLength */
	USB_DESC_TYPE_STRING,                           /* bDescriptorType */
	'R',                                            /* bString */
	'e',
	'a',
	'l',
	't',
	'e',
	'k',
	' ',
	'V',
	'C',
	'P',
	' ',
	'(',
	'F',
	'S',
	')',
};  /* usbd_msc_fs_product_string_desc */

/* USB Standard SN String Descriptor, ASCII only */
static u8 usbd_msc_sn_string_desc[USBD_MSC_SN_STRING_DESC_SIZE] USB_DMA_ALIGNED = {
	USBD_MSC_SN_STRING_DESC_SIZE,                    /* bLength */
	USB_DESC_TYPE_STRING,                           /* bDescriptorType */
	'1',                                            /* bString */
	'\0',
	'2',
	'\0',
	'3',
	'\0',
	'4',
	'\0',
	'5',
	'\0',
	'6',
	'\0',
	'7',
	'\0',
	'8',
	'\0',
	'9',
	'\0',
	'0',
	'\0',
	'A',
	'\0',
	'B',
	'\0'
};  /* usbd_msc_sn_string_desc */

/* USB Standard Device Qualifier Descriptor */
static u8 usbd_msc_device_qualifier_desc[USB_LEN_DEV_QUALIFIER_DESC] USB_DMA_ALIGNED = {
	USB_LEN_DEV_QUALIFIER_DESC,                     /* bLength */
	USB_DESC_TYPE_DEVICE_QUALIFIER,                 /* bDescriptorType */
	0x00,                                           /* bcdUSB */
	0x02,
	0x00,                                           /* bDeviceClass */
	0x00,                                           /* bDeviceSubClass */
	0x00,                                           /* bDeviceProtocol */
	0x40,                                           /* bMaxPacketSize */
	0x01,                                           /* bNumConfigurations */
	0x00,                                           /* Reserved */
};  /* usbd_msc_device_qualifier_desc */

/* USB MSC Device High Speed Configuration Descriptor */
static u8 usbd_msc_hs_config_desc[USBD_MSC_CONFIG_DESC_SIZE] USB_DMA_ALIGNED = {
	/* USB Standard Configuration Descriptor */
	USB_LEN_CFG_DESC,                               /* bLength */
	USB_DESC_TYPE_CONFIGURATION,                    /* bDescriptorType */
	USBD_MSC_CONFIG_DESC_SIZE,                       /* wTotalLength */
	0x00,
	0x01,                                           /* bNumInterfaces */
	0x01,                                           /* bConfigurationValue */
	0x00,                                           /* iConfiguration */
#if USBD_MSC_SELF_POWERED
	0xC0,                                           /* bmAttributes: self powered */
#else
	0x80,                                           /* bmAttributes: bus powered */
#endif
	0x32,                                           /* bMaxPower */

	/********************  Mass Storage interface ********************/
	USB_LEN_IF_DESC,			/* bLength: Interface Descriptor size */
	USB_DESC_TYPE_INTERFACE,	/* bDescriptorType: */
	0x00,						/* bInterfaceNumber: Number of Interface */
	0x00,						/* bAlternateSetting: Alternate setting */
	0x02,						/* bNumEndpoints*/
	0x08,						/* bInterfaceClass: MSC Class */
	0x06,						/* bInterfaceSubClass : SCSI transparent*/
	0x50,						/* nInterfaceProtocol */
	0x00,						/* iInterface: */

	/* BULK OUT Endpoint Descriptor */
	USB_LEN_EP_DESC,			/* bLength */
	USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType */
	USBD_MSC_BULK_OUT_EP,		/* bEndpointAddress */
	0x02,						/* bmAttributes: BULK */
	USB_LOW_BYTE(USBD_MSC_HS_MAX_PACKET_SIZE),  /* wMaxPacketSize: */
	USB_HIGH_BYTE(USBD_MSC_HS_MAX_PACKET_SIZE),
	0x00,						/* bInterval */

	/* BULK IN Endpoint Descriptor */
	USB_LEN_EP_DESC,			/* bLength */
	USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType */
	USBD_MSC_BULK_IN_EP,		/* bEndpointAddress */
	0x02,						/* bmAttributes: BULK */
	USB_LOW_BYTE(USBD_MSC_HS_MAX_PACKET_SIZE),  /* wMaxPacketSize: */
	USB_HIGH_BYTE(USBD_MSC_HS_MAX_PACKET_SIZE),
	0x00                                            /* bInterval */
};  /* usbd_msc_hs_config_desc */

/* USB MSC Device Full Speed Configuration Descriptor */
static u8 usbd_msc_fs_config_desc[USBD_MSC_CONFIG_DESC_SIZE] USB_DMA_ALIGNED = {
	/* USB Standard Configuration Descriptor */
	USB_LEN_CFG_DESC,                               /* bLength */
	USB_DESC_TYPE_CONFIGURATION,                    /* bDescriptorType */
	USBD_MSC_CONFIG_DESC_SIZE,                       /* wTotalLength */
	0x00,
	0x01,                                           /* bNumInterfaces */
	0x01,                                           /* bConfigurationValue */
	0x00,                                           /* iConfiguration */
#if USBD_MSC_SELF_POWERED
	0xC0,                                           /* bmAttributes: self powered */
#else
	0x80,                                           /* bmAttributes: bus powered */
#endif
	0x32,                                           /* bMaxPower */

	/********************  Mass Storage interface ********************/
	USB_LEN_IF_DESC,			/* bLength: Interface Descriptor size */
	USB_DESC_TYPE_INTERFACE,	/* bDescriptorType: */
	0x00,						/* bInterfaceNumber: Number of Interface */
	0x00,						/* bAlternateSetting: Alternate setting */
	0x02,						/* bNumEndpoints*/
	0x08,						/* bInterfaceClass: MSC Class */
	0x06,						/* bInterfaceSubClass : SCSI transparent*/
	0x50,						/* nInterfaceProtocol */
	0x00,						/* iInterface: */

	/* BULK OUT Endpoint Descriptor */
	USB_LEN_EP_DESC,			/* bLength */
	USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType */
	USBD_MSC_BULK_OUT_EP,		/* bEndpointAddress */
	0x02,						/* bmAttributes: BULK */
	USB_LOW_BYTE(USBD_MSC_FS_MAX_PACKET_SIZE),  /* wMaxPacketSize: */
	USB_HIGH_BYTE(USBD_MSC_FS_MAX_PACKET_SIZE),
	0x00,						/* bInterval */

	/* BULK IN Endpoint Descriptor */
	USB_LEN_EP_DESC,			/* bLength */
	USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType */
	USBD_MSC_BULK_IN_EP,		/* bEndpointAddress */
	0x02,						/* bmAttributes: BULK */
	USB_LOW_BYTE(USBD_MSC_FS_MAX_PACKET_SIZE),  /* wMaxPacketSize: */
	USB_HIGH_BYTE(USBD_MSC_FS_MAX_PACKET_SIZE),
	0x00                                            /* bInterval */
};  /* usbd_msc_fs_config_desc */

/* USB MSC Device Other Speed Configuration Descriptor */
static u8 usbd_msc_other_speed_config_desc[USBD_MSC_CONFIG_DESC_SIZE] USB_DMA_ALIGNED = {
	/* USB Standard Configuration Descriptor */
	USB_LEN_CFG_DESC,                               /* bLength */
	USB_DESC_TYPE_OTHER_SPEED_CONFIGURATION,	/* bDescriptorType */
	USBD_MSC_CONFIG_DESC_SIZE,                       /* wTotalLength */
	0x00,
	0x01,                                           /* bNumInterfaces */
	0x01,                                           /* bConfigurationValue */
	0x00,                                           /* iConfiguration */
#if USBD_MSC_SELF_POWERED
	0xC0,                                           /* bmAttributes: self powered */
#else
	0x80,                                           /* bmAttributes: bus powered */
#endif
	0x32,                                           /* bMaxPower */

	/********************  Mass Storage interface ********************/
	USB_LEN_IF_DESC,			/* bLength: Interface Descriptor size */
	USB_DESC_TYPE_INTERFACE,	/* bDescriptorType: */
	0x00,						/* bInterfaceNumber: Number of Interface */
	0x00,						/* bAlternateSetting: Alternate setting */
	0x02,						/* bNumEndpoints*/
	0x08,						/* bInterfaceClass: MSC Class */
	0x06,						/* bInterfaceSubClass : SCSI transparent*/
	0x50,						/* nInterfaceProtocol */
	0x00,						/* iInterface: */

	/* BULK OUT Endpoint Descriptor */
	USB_LEN_EP_DESC,			/* bLength */
	USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType */
	USBD_MSC_BULK_OUT_EP,		/* bEndpointAddress */
	0x02,						/* bmAttributes: BULK */
	USB_LOW_BYTE(USBD_MSC_FS_MAX_PACKET_SIZE),  /* wMaxPacketSize: */
	USB_HIGH_BYTE(USBD_MSC_FS_MAX_PACKET_SIZE),
	0x00,						/* bInterval */

	/* BULK IN Endpoint Descriptor */
	USB_LEN_EP_DESC,			/* bLength */
	USB_DESC_TYPE_ENDPOINT,	/* bDescriptorType */
	USBD_MSC_BULK_IN_EP,		/* bEndpointAddress */
	0x02,						/* bmAttributes: BULK */
	USB_LOW_BYTE(USBD_MSC_FS_MAX_PACKET_SIZE),  /* wMaxPacketSize: */
	USB_HIGH_BYTE(USBD_MSC_FS_MAX_PACKET_SIZE),
	0x00                                            /* bInterval */
};  /* usbd_msc_other_speed_config_desc */

/* MSC Class Driver */
usbd_class_driver_t usbd_msc_driver = {
	.get_descriptor = usbd_msc_get_descriptor,
	.set_config = usbd_msc_set_config,
	.clear_config = usbd_msc_clear_config,
	.setup = usbd_msc_setup,
	.ep_data_in = usbd_msc_handle_ep_data_in,
	.ep_data_out = usbd_msc_handle_ep_data_out,
};

// physical disk access methods
static usbd_msc_dev_t usbd_msc_dev;

/* Private functions ---------------------------------------------------------*/

#if USBD_MSC_RAM_DISK
static u8 *usbd_msc_ram_disk_buf;

static SD_RESULT RAM_init(void)
{
	SD_RESULT result = SD_OK;
	usbd_msc_ram_disk_buf = (u8 *)rtw_zmalloc(USBD_MSC_RAM_DISK_SIZE);
	if (usbd_msc_ram_disk_buf == NULL) {
		DBG_PRINTF(MODULE_USB_CLASS, LEVEL_ERROR, "fail to allocate RAM disk buffer");
		result = SD_NODISK;
	}
	return result;
}

static SD_RESULT RAM_deinit(void)
{
	if (usbd_msc_ram_disk_buf != NULL) {
		rtw_free((void *)usbd_msc_ram_disk_buf);
		usbd_msc_ram_disk_buf = NULL;
	}
	return SD_OK;
}

static SD_RESULT RAM_GetCapacity(u32 *sector_count)
{
	*sector_count = USBD_MSC_RAM_DISK_SECTORS;
	return SD_OK;
}

static SD_RESULT RAM_ReadBlocks(u32 sector, u8 *data, u32 count)
{
	SD_RESULT result = SD_ERROR;
	if (sector + count <= USBD_MSC_RAM_DISK_SECTORS) {
		rtw_memcpy((void *)data, (void *)(usbd_msc_ram_disk_buf + sector * USBD_MSC_BLK_SIZE), count * USBD_MSC_BLK_SIZE);
		result = SD_OK;
	}
	return result;
}

static SD_RESULT RAM_WriteBlocks(u32 sector, const u8 *data, u32 count)
{
	SD_RESULT result = SD_ERROR;
	if (sector + count <= USBD_MSC_RAM_DISK_SECTORS) {
		rtw_memcpy((void *)(usbd_msc_ram_disk_buf + sector * USBD_MSC_BLK_SIZE), (void *)data, count * USBD_MSC_BLK_SIZE);
		result = SD_OK;
	}
	return result;
}

#endif // USBD_MSC_RAM_DISK

/**
* @brief  Send the Command Status Wrapper
* @param  dev: device instance
* @param  status : CSW status
* @retval None
*/
void  usbd_msc_send_csw(usb_dev_t *dev, u8 status)
{
	usbd_msc_dev_t *cdev = &usbd_msc_dev;

	cdev->csw.Signature = USBD_MSC_CS_SIGN;
	cdev->csw.Status = status;
	cdev->bot_state = USBD_MSC_IDLE;

	usbd_ep_transmit(dev, USBD_MSC_BULK_IN_EP, (u8 *)&cdev->csw, USBD_MSC_CS_WRAP_LEN);

	/* Prepare EP to Receive next Cmd */
	usbd_ep_receive(dev, USBD_MSC_BULK_OUT_EP, (u8 *)&cdev->cbw, USBD_MSC_CB_WRAP_LEN);
}

/**
* @brief  Abort the current transfer
* @param  dev: device instance
* @retval status
*/

static void usbd_msc_abort(usb_dev_t *dev)
{
	usbd_msc_dev_t *cdev = &usbd_msc_dev;

	if ((cdev->cbw.Flags == 0U) &&
		(cdev->cbw.DataTransferLength != 0U) &&
		(cdev->bot_status == USBD_MSC_STATUS_NORMAL)) {
		usbd_pcd_ep_set_stall(dev->pcd, USBD_MSC_BULK_OUT_EP);
	}

	usbd_pcd_ep_set_stall(dev->pcd, USBD_MSC_BULK_IN_EP);

	if (cdev->bot_status == USBD_MSC_STATUS_ERROR) {
		usbd_ep_receive(dev, USBD_MSC_BULK_OUT_EP, (u8 *)&cdev->cbw, USBD_MSC_CB_WRAP_LEN);
	}
}

/**
  * @brief  Set MSC class configuration
  * @param  dev: USB device instance
  * @param  config: USB configuration index
  * @retval status
  */
static u8 usbd_msc_set_config(usb_dev_t *dev, u8 config)
{
	u16 ep_mps;
	u8 ret = HAL_OK;
	usbd_msc_dev_t *cdev = &usbd_msc_dev;

	UNUSED(config);

	cdev->dev = dev;

	/* Init BULK IN EP */
	ep_mps = (dev->dev_speed == USBD_SPEED_HIGH) ? USBD_MSC_HS_MAX_PACKET_SIZE : USBD_MSC_FS_MAX_PACKET_SIZE;
	usbd_ep_init(dev, USBD_MSC_BULK_IN_EP, USBD_EP_TYPE_BULK, ep_mps);

	/* Init BULK OUT EP */
	ep_mps = (dev->dev_speed == USBD_SPEED_HIGH) ? USBD_MSC_HS_MAX_PACKET_SIZE : USBD_MSC_FS_MAX_PACKET_SIZE;
	usbd_ep_init(dev, USBD_MSC_BULK_OUT_EP, USBD_EP_TYPE_BULK, ep_mps);

	/* Prepare to receive next BULK OUT packet */
	usbd_ep_receive(dev, USBD_MSC_BULK_OUT_EP, (u8 *)&cdev->cbw, USBD_MSC_CB_WRAP_LEN);

	return ret;
}

/**
  * @brief  Clear MSC configuration
  * @param  dev: USB device instance
  * @param  config: USB configuration index
  * @retval status
  */
static u8 usbd_msc_clear_config(usb_dev_t *dev, u8 config)
{
	u8 ret = 0U;

	UNUSED(config);

	/* DeInit BULK IN EP */
	usbd_ep_deinit(dev, USBD_MSC_BULK_IN_EP);

	/* DeInit BULK OUT EP */
	usbd_ep_deinit(dev, USBD_MSC_BULK_OUT_EP);

	return ret;
}

/**
  * @brief  Handle MSC specific CTRL requests
  * @param  dev: USB device instance
  * @param  req: USB CTRL requests
  * @retval status
  */
static u8 usbd_msc_setup(usb_dev_t *dev, usb_setup_req_t *req)
{
	usbd_msc_dev_t *cdev = &usbd_msc_dev;
	u8 ret = HAL_OK;
	u16 ep_mps;

	DBG_PRINTF(MODULE_USB_CLASS,
			   LEVEL_TRACE,
			   "usbd_msc_setup bmRequestType=0x%02X bRequest=0x%02X wLength=0x%04X wValue=%x\n",
			   req->bmRequestType,
			   req->bRequest,
			   req->wLength,
			   req->wValue);

	switch (req->bmRequestType & USB_REQ_TYPE_MASK) {
	/* Class request */
	case USB_REQ_TYPE_CLASS:
		switch (req->bRequest) {
		case USBD_MSC_REQUEST_GET_MAX_LUN:
			if ((req->wValue  == 0U) && (req->wLength == 1U) &&
				((req->bmRequestType & 0x80U) == 0x80U)) {
				dev->ctrl_buf[0] = 0U;
				usbd_ep0_transmit(dev, dev->ctrl_buf, 1U);
			} else {
				usbd_core_ep0_error(dev);
				ret = HAL_ERR_HW;
			}
			break;

		case USBD_MSC_REQUEST_RESET :
			if ((req->wValue  == 0U) && (req->wLength == 0U) &&
				((req->bmRequestType & 0x80U) != 0x80U)) {
				cdev->bot_state  = USBD_MSC_IDLE;
				cdev->bot_status = USBD_MSC_STATUS_RECOVERY;
				/* Prepare to receive BOT cmd */
				usbd_ep_receive(dev, USBD_MSC_BULK_OUT_EP, (u8 *)&cdev->cbw, USBD_MSC_CB_WRAP_LEN);
			} else {
				usbd_core_ep0_error(dev);
				ret = HAL_ERR_HW;
			}
			break;

		default:
			usbd_core_ep0_error(dev);
			ret = HAL_ERR_HW;
			break;
		}
		break;
	/* Interface & Endpoint request */
	case USB_REQ_TYPE_STANDARD:
		switch (req->bRequest) {
		case USB_REQ_GET_STATUS:
			if (dev->dev_state == USBD_STATE_CONFIGURED) {
				dev->ctrl_buf[0] = 0U;
				dev->ctrl_buf[1] = 0U;
				usbd_ep0_transmit(dev, dev->ctrl_buf, 2U);
			} else {
				usbd_core_ep0_error(dev);
				ret = HAL_ERR_HW;
			}
			break;

		case USB_REQ_GET_INTERFACE:
			if (dev->dev_state == USBD_STATE_CONFIGURED) {
				usbd_ep0_transmit(dev, (u8 *)&cdev->intf, 1U);
			} else {
				usbd_core_ep0_error(dev);
				ret = HAL_ERR_HW;
			}
			break;

		case USB_REQ_SET_INTERFACE:
			if (dev->dev_state == USBD_STATE_CONFIGURED) {
				cdev->intf = (u8)(req->wValue);
			} else {
				usbd_core_ep0_error(dev);
				ret = HAL_ERR_HW;
			}
			break;

		case USB_REQ_CLEAR_FEATURE:
			/* DeInit EP */
			usbd_ep_deinit(dev, (u8)req->wIndex);
			if ((((u8)req->wIndex) & 0x80U) == 0x80U) {
				ep_mps = (dev->dev_speed == USBD_SPEED_HIGH) ? USBD_MSC_HS_MAX_PACKET_SIZE : USBD_MSC_FS_MAX_PACKET_SIZE;
				usbd_ep_init(dev, USBD_MSC_BULK_IN_EP, USBD_EP_TYPE_BULK, ep_mps);
			} else {
				ep_mps = (dev->dev_speed == USBD_SPEED_HIGH) ? USBD_MSC_HS_MAX_PACKET_SIZE : USBD_MSC_FS_MAX_PACKET_SIZE;
				usbd_ep_init(dev, USBD_MSC_BULK_OUT_EP, USBD_EP_TYPE_BULK, ep_mps);
			}

			/* Handle BOT error */
			if (cdev->bot_status == USBD_MSC_STATUS_ERROR) { /* Bad CBW Signature */
				usbd_pcd_ep_set_stall(dev->pcd, USBD_MSC_BULK_IN_EP);
				cdev->bot_status = USBD_MSC_STATUS_NORMAL;
			} else if (((((u8)req->wIndex) & 0x80U) == 0x80U) && (cdev->bot_status != USBD_MSC_STATUS_RECOVERY)) {
				usbd_msc_send_csw(dev, USBD_MSC_CSW_CMD_FAILED);
			} else {
				;
			}
			break;

		default:
			usbd_core_ep0_error(dev);
			ret = HAL_ERR_HW;
			break;
		}
		break;

	default:
		usbd_core_ep0_error(dev);
		ret = HAL_ERR_HW;
		break;
	}

	return ret;
}

/**
  * @brief  usbd_msc_handle_ep_data_in
  *         Data sent on non-control IN endpoint
  * @param  dev: USB device instance
  * @param  ep_num: endpoint number
  * @retval status
  */
static u8 usbd_msc_handle_ep_data_in(usb_dev_t *dev, u8 ep_num)
{
	usbd_msc_dev_t *cdev = &usbd_msc_dev;

	UNUSED(ep_num);

	switch (cdev->bot_state) {
	case USBD_MSC_DATA_IN:
		if (usbd_scsi_process_cmd(cdev, &cdev->cbw.CDB[0]) < 0) {
			usbd_msc_send_csw(dev, USBD_MSC_CSW_CMD_FAILED);
		}
		break;

	case USBD_MSC_SEND_DATA:
	case USBD_MSC_LAST_DATA_IN:
		usbd_msc_send_csw(dev, USBD_MSC_CSW_CMD_PASSED);
		break;

	default:
		break;
	}

	return HAL_OK;
}

/**
  * @brief  usbd_msc_handle_ep_data_out
  *         Data received on non-control Out endpoint
  * @param  dev: USB device instance
  * @param  ep_num: endpoint number
  * @retval status
  */
static u8 usbd_msc_handle_ep_data_out(usb_dev_t *dev, u8 ep_num, u16 len)
{
	usbd_msc_dev_t *cdev = &usbd_msc_dev;

	UNUSED(ep_num);
	UNUSED(len);

	switch (cdev->bot_state) {
	case USBD_MSC_IDLE:
		/* Decode the CBW command */
		cdev->csw.Tag = cdev->cbw.Tag;
		cdev->csw.Residue = cdev->cbw.DataTransferLength;

		if ((usbd_core_get_rx_data_size(dev, USBD_MSC_BULK_OUT_EP) != USBD_MSC_CB_WRAP_LEN) ||
			(cdev->cbw.Signature != USBD_MSC_CB_SIGN) ||
			(cdev->cbw.Lun > 1U) ||
			(cdev->cbw.Length < 1U) || (cdev->cbw.Length > 16U)) {
			usbd_scsi_sense_code(cdev, ILLEGAL_REQUEST, INVALID_CDB);
			cdev->bot_status = USBD_MSC_STATUS_ERROR;
			usbd_msc_abort(dev);
		} else {
			if (usbd_scsi_process_cmd(cdev, &cdev->cbw.CDB[0]) < 0) {
				if (cdev->bot_state == USBD_MSC_NO_DATA) {
					usbd_msc_send_csw(dev, USBD_MSC_CSW_CMD_FAILED);
				} else {
					usbd_msc_abort(dev);
				}
			}
			/*Burst xfer handled internally*/
			else if ((cdev->bot_state != USBD_MSC_DATA_IN) &&
					 (cdev->bot_state != USBD_MSC_DATA_OUT) &&
					 (cdev->bot_state != USBD_MSC_LAST_DATA_IN)) {
				if (cdev->data_length > 0U) {
					u16 length = (u16)MIN(cdev->cbw.DataTransferLength, cdev->data_length);
					cdev->csw.Residue -= cdev->data_length;
					cdev->csw.Status = USBD_MSC_CSW_CMD_PASSED;
					cdev->bot_state = USBD_MSC_SEND_DATA;

					usbd_ep_transmit(dev, USBD_MSC_BULK_IN_EP, cdev->data, length);
				} else if (cdev->data_length == 0U) {
					usbd_msc_send_csw(dev, USBD_MSC_CSW_CMD_PASSED);
				} else {
					usbd_msc_abort(dev);
				}
			} else {
				return HAL_OK;
			}
		}
		break;

	case USBD_MSC_DATA_OUT:
		if (usbd_scsi_process_cmd(cdev, &cdev->cbw.CDB[0]) < 0) {
			usbd_msc_send_csw(dev, USBD_MSC_CSW_CMD_FAILED);
		}

		break;

	default:
		break;
	}

	return HAL_OK;
}

static u8 *usbd_msc_get_descriptor(usbd_desc_type_t desc, usbd_speed_type_t speed, u16 *len)
{
	u8 *buf = NULL;
	switch (desc) {
	case USBD_DESC_DEVICE:
		buf = usbd_msc_dev_desc;
		*len = sizeof(usbd_msc_dev_desc);
		break;
	case USBD_DESC_CONFIG:
		if (speed == USBD_SPEED_HIGH) {
			buf = usbd_msc_hs_config_desc;
			*len = sizeof(usbd_msc_hs_config_desc);
		} else {
			buf = usbd_msc_fs_config_desc;
			*len = sizeof(usbd_msc_fs_config_desc);
		}
		break;
	case USBD_DESC_OTHER_SPEED_CONFIG:
		/* speed == USBD_SPEED_HIGH */
		buf = usbd_msc_other_speed_config_desc;
		*len = sizeof(usbd_msc_other_speed_config_desc);
		break;
	case USBD_DESC_DEVICE_QUALIFIER:
		/* speed == USBD_SPEED_HIGH */
		buf = usbd_msc_device_qualifier_desc;
		*len = sizeof(usbd_msc_device_qualifier_desc);
		break;
	case USBD_DESC_LANGID_STR:
		buf = usbd_msc_lang_id_desc;
		*len = sizeof(usbd_msc_lang_id_desc);
		break;
	case USBD_DESC_MFG_STR:
		buf = usbd_msc_mfg_string_desc;
		*len = USBD_MSC_MFG_STRING_DESC_SIZE;
		break;
	case USBD_DESC_PRODUCT_STR:
		*len = USBD_MSC_PRODUCT_STRING_DESC_SIZE;
		if (speed == USBD_SPEED_HIGH) {
			buf = usbd_msc_hs_product_string_desc;
		} else {
			buf = usbd_msc_fs_product_string_desc;
		}
		break;
	case USBD_DESC_SN_STR:
		buf = usbd_msc_sn_string_desc;
		*len = USBD_MSC_SN_STRING_DESC_SIZE;
		break;
	default:
		break;
	}

	return buf;
}

/* Exported functions --------------------------------------------------------*/

int usbd_msc_init(void)
{
	usbd_msc_dev_t *cdev = &usbd_msc_dev;
	int ret = HAL_OK;

#if USBD_MSC_RAM_DISK
	cdev->disk_operation.disk_init = RAM_init;
	cdev->disk_operation.disk_deinit = RAM_deinit;
	cdev->disk_operation.disk_getcapacity = RAM_GetCapacity;
	cdev->disk_operation.disk_read = RAM_ReadBlocks;
	cdev->disk_operation.disk_write = RAM_WriteBlocks;
#else
	cdev->disk_operation.disk_init = SD_Init;
	cdev->disk_operation.disk_deinit = SD_DeInit;
	cdev->disk_operation.disk_getcapacity = SD_GetCapacity;
	cdev->disk_operation.disk_read = SD_ReadBlocks;
	cdev->disk_operation.disk_write = SD_WriteBlocks;
#endif

	cdev->data = rtw_zmalloc(USBD_MSC_BUFLEN);
	if (cdev->data == NULL) {
		ret = HAL_ERR_MEM;
		goto data_buf_fail;
	}

	cdev->bot_state = USBD_MSC_IDLE;
	cdev->bot_status = USBD_MSC_STATUS_NORMAL;
	cdev->scsi_sense_tail = 0U;
	cdev->scsi_sense_head = 0U;
	if (cdev->disk_operation.disk_init()) {
		DBG_PRINTF(MODULE_USB_CLASS, LEVEL_ERROR, "disk init fail\n");
		rtw_free(cdev->data);
		cdev->data = NULL;
		return HAL_ERR_HW;
	}
	cdev->is_open = 1;
	cdev->ro = 0;
	cdev->blkbits = USBD_MSC_BLK_BITS;
	cdev->blksize = USBD_MSC_BLK_SIZE;
	usbd_register_class(&usbd_msc_driver);

data_buf_fail:
	return ret;
}

void usbd_msc_deinit(void)
{
	usbd_msc_dev_t *cdev = &usbd_msc_dev;

	cdev->bot_state  = USBD_MSC_IDLE;
	usbd_unregister_class();

	if (cdev->disk_operation.disk_deinit != NULL) {
		cdev->disk_operation.disk_deinit();
	}
	cdev->is_open = 0;

	if (cdev->data != NULL) {
		rtw_free(cdev->data);
		cdev->data = NULL;
	}
}

