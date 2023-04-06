##################################################################################
#                                                                                #
#                             Example example_usbh_uvc                           #
#                                                                                #
##################################################################################

Date: 2020-02-24

Table of Contents
~~~~~~~~~~~~~~~~~
 - Description
 - Setup Guide
 - Parameter Setting and Configuration
 - Result description
 - Other Reference
 - Supported List

 
Description
~~~~~~~~~~~
        With USB host v2.0 interface, Ameba can be designed to a USB UVC host.
        In this application, Ameba boots up as USB UVC host, detects USB UVC device 
        attached to it, and optionally:
        1. Captures images and save them into SD card as jpeg files.
        2. Captures images and send them to HTTP(s) server as jpeg files.
        3. Capture images and response with the realtime image to the GET request of HTTP(s) client.
        4. Capture video stream and response with the realtime video stream to request of the RTSP client.
    
Setup Guide
~~~~~~~~~~~
        In order to run this application successfully, the hardware setup should be confirm before moving on.
        1. Connect USB camera, which shall be compliant with UVC and integrate MJPEG encoder, to the micro USB connector on Ameba develop board.
        2. For GCC environmnet, type command "make menuconfig" and choose CONFIG USB => Enable USB => USB OTG Type => 
           USB_OTG_HOST_MODE and then choose USB Host Type => USB_HOST_UVC,
           this will auto-generate following lines in platform_autoconf.h:
               #define CONFIG_USB_OTG_EN              1
               #define CONFIG_USB_HOST_EN             1
               #define CONFIG_USBH_UVC                1
               #define CONFIG_V4L2_EN                 1
               #define CONFIG_RTSP_EN                 1
               #define CONFIG_RTP_CODEC_EN            1
               #define CONFIG_MMF_EN                  1
           Note that CONFIG_V4L2_EN/CONFIG_RTSP_EN/CONFIG_RTP_CODEC_EN/CONFIG_MMF_EN is to 1 herein since CONFIG_USBH_UVC_APP is 
           set to USBH_UVC_APP_MMF_RTSP in example_usbh_uvc.c as default, user could manually unset CONFIG_RTSP_EN/CONFIG_RTP_CODEC_EN/CONFIG_MMF_EN 
           if CONFIG_USBH_UVC_APP is set to other values in example_usbh_uvc.c.
           And for IAR environment, manually edit platform_autoconf.h as above.
        3. Make sure the USBH UVC example is enabled (default enabled only if CONFIG_USBH_UVC is defined) in platform_opts.h:
               #if defined(CONFIG_USBH_UVC)
               #define CONFIG_VIDEO_APPLICATION       1
               #define CONFIG_EXAMPLE_USBH_UVC        1
               #if CONFIG_EXAMPLE_USBH_UVC
               #define CONFIG_FATFS_EN                0
               #if CONFIG_FATFS_EN
               // fatfs version
               #define FATFS_R_10C
               // fatfs disk interface
               #define FATFS_DISK_USB                 0
               #define FATFS_DISK_SD                  1
               #define FATFS_DISK_FLASH 	          0
               #endif
               #endif
               #endif
           Specially please set CONFIG_FATFS_EN to 1 herein if CONFIG_USBH_UVC_APP is set to USBH_UVC_APP_FATFS in example_usbh_uvc.c.
        4. If CONFIG_USBH_UVC_APP is set to USBH_UVC_APP_FATFS in example_usbh_uvc.c, configure FATFS as below in ffconf.h:
               #define  _MIN_SS                       512   // Define the minimum sector size supported.
               #define  _MAX_SS                       512   // Define the maximum sector size supported.
		       #define  _USE_MKFS                     0     // Disable f_mkfs() function.
		5. If CONFIG_USBH_UVC_APP is set to USBH_UVC_APP_HTTPD in example_usbh_uvc.c, configure LWIP as below in lwipopts.h:
               #define SO_REUSE                       1
           Specially if CONFIG_USBH_UVC_USE_HTTPS is set to 1 in example_usbh_uvc.c, please configure MBEDTLS/SSL as below in config_rsa.h:
               #define MBEDTLS_CERTS_C
               #define MBEDTLS_SSL_SRV_C
        6. Enable PSRAM in rtl8721dhp_intfcfg.c:
               PSRAMCFG_TypeDef psram_dev_config = {
                   .psram_dev_enable = TRUE,            //enable psram
                   .psram_dev_cal_enable = TRUE,        //enable psram calibration function
                   .psram_dev_retention = TRUE,         //enable psram retention
               };
        7. Please make sure to reserve enough text/heap size for UVC by adjusting configTOTAL_HEAP_SIZE in freeRTOSconfig.h and/or turning off some functions
           (e.g. WPS, JDSMART, ATcmd for internal and system) since UVC functions will consume more RAM space than default configuration. 
           Besides, please make sure to reserve enough heap size in PSRAM for UVC by adjusting configTOTAL_PSRAM_HEAP_SIZE in psram_reserve.c (refer to PSRAM AN for details), 
           since image frame resident in PSRAM will consume quite large memory space.
        8. Specially for IAR environment, manually include lib/lib_usbh.a (default excluded) and lib/lib_v4l2.a (default excluded) to 
           and exclude lib/lib_usbd.a (default excluded) from the build of km4_application project, specially, please also manually include
           lib/lib_rtsp.a, lib/lib_rtp_codec.a and lib/lib_mmfv2.a (default excluded) to the build of km4_application project if CONFIG_USBH_UVC_APP
           is set to USBH_UVC_APP_MMF_RTSP in example_usbh_uvc.c as default.
        9. Rebuild the project and download firmware to Ameba develop board.
        10. Reset and check the log via serial console.

Parameter Setting and Configuration
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
        Adjust CONFIG_USBH_UVC_APP to configure the UVC application in example_usbh_uvc.c:
               // Capture UVC images from the camera and save as jpeg files into SD card
               #define USBH_UVC_APP_FATFS             0
               // Use EVB as HTTP(s) server to allow HTTP(s) clients to GET UVC image captured from the camera
               #define USBH_UVC_APP_HTTPD             1
               // Use EVB as HTTP(s) client to PUT UVC image captured from the camera to HTTP(s) server
               #define USBH_UVC_APP_HTTPC             2
               // Use MMF with UVC as video source and RTSP as sink
               #define USBH_UVC_APP_MMF_RTSP          3

        Adjust CONFIG_USBH_UVC_USE_HTTPS to enable/disable HTTPS when CONFIG_USBH_UVC_APP is set to USBH_UVC_APP_HTTPD 
        or USBH_UVC_APP_HTTPC in example_usbh_uvc.c:
               // Use EVB as HTTPS client to POST UVC image captured from the camera to HTTPS server
               #define CONFIG_USBH_UVC_USE_HTTPS      0
    
Result description
~~~~~~~~~~~~~~~~~~
        On the serial console, USB UVC host loading and image capturing log will be printed, make sure there is no error reported.
        1. For CONFIG_USBH_UVC_APP == USBH_UVC_APP_FATFS:
           The images will be saved into SD card attached to the Ameba develop board, which can be checked via card reader.
           For time lapse assembling, please run ffmpeg.bat after images saved.
        2. For CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPD:
           If CONFIG_USBH_UVC_USE_HTTPS == 0: access the current image via http://<ip_of_ameba_develop_board> in web browser.
           If CONFIG_USBH_UVC_USE_HTTPS == 1: access the current image via https://<ip_of_ameba_develop_board> in web browser.
        3. For CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPC:
           The images will be put into uploads directory under the web root path of http(s) server.
           For time lapse assembling, please run ffmpeg.bat after images uploaded.
        4. For CONFIG_USBH_UVC_APP == USBH_UVC_APP_MMF_RTSP:
           The video stream can be accessed via RTSP client such as a video player on PC.

Other Reference
~~~~~~~~~~~~~~~
        None.

Supported List
~~~~~~~~~~~~~~
[Supported List]
        Supported : Ameba-D
        Source code not in project: Ameba-1, Ameba-z, Ameba-pro
