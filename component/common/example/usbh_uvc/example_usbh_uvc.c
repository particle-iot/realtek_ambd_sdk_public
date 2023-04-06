#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBH_UVC) && CONFIG_EXAMPLE_USBH_UVC

#include "example_usbh_uvc.h"
#include "uvcvideo.h"
#include "uvc_intf.h"
#include "section_config.h"

// USBH UVC configurations
// Use EVB as HTTP(s) server to allow HTTP(s) clients to GET UVC image captured from the camera
#define USBH_UVC_APP_HTTPD               0
// Use EVB as HTTP(s) client to PUT UVC image captured from the camera to HTTP(s) server
#define USBH_UVC_APP_HTTPC               1
// Capture UVC images from the camera and save as jpeg files into SD card
#define USBH_UVC_APP_FATFS               2
// Use MMF with UVC as video source and RTSP as sink
#define USBH_UVC_APP_MMF_RTSP            3

#define CONFIG_USBH_UVC_APP              USBH_UVC_APP_MMF_RTSP

// Use EVB as HTTPS client to POST UVC image captured from the camera to HTTPS server
#define CONFIG_USBH_UVC_USE_HTTPS        0

// Check whether the captured image data is valid before futher process
#define CONFIG_USBH_UVC_CHECK_IMAGE_DATA 0

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPD)
#include "lwip/sockets.h"
#include <httpd/httpd.h>
#if CONFIG_USBH_UVC_USE_HTTPS
#if (HTTPD_USE_TLS == HTTPD_TLS_POLARSSL)
#include <polarssl/certs.h>
#elif (HTTPD_USE_TLS == HTTPD_TLS_MBEDTLS)
#include <mbedtls/certs.h>
#endif
#endif
#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPC)
#include <httpc/httpc.h>
#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_FATFS)
#include "ff.h"
#include <fatfs_ext/inc/ff_driver.h>
#include <disk_if/inc/sdcard.h>
#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_MMF_RTSP)
#include "mmf2.h"
#include "mmf2_dbg.h"
#include "module_uvch.h"
#include "module_rtsp2.h"
#endif

#define USBH_UVC_BUF_SIZE       200000U   // Frame buffer size, resident in PSRAM, depends on format type, resolution and compression ratio
#define USBH_UVC_FORMAT_TYPE    0         // UVC_FORMAT_MJPEG
#define USBH_UVC_WIDTH          1280
#define USBH_UVC_HEIGHT         720
#define USBH_UVC_FRAME_RATE     15
#define USBH_UVC_COMP_RATIO     0

#if UVC_BUF_DYNAMIC                       // UVC_BUF_DYNAMIC = 1 as default, to allow user to full control the PSRAM usage
#define USBH_UVC_REQBUF_NUM     4         // UVC request buffer number, UVC PSRAM size = USBH_UVC_BUF_SIZE + USBH_UVC_REQBUF_SIZE * USBH_UVC_REQBUF_NUM
#define USBH_UVC_REQBUF_SIZE    200000U  // UVC request buffer size, shall be >= USBH_UVC_BUF_SIZE, resident in PSRAM
#endif

#if (USBH_UVC_FORMAT_TYPE != 0)
#error "USBH UVC unsupported format, only MJPEG is supported"
#endif

#define USBH_UVC_JFIF_TAG       0xFF
#define USBH_UVC_JFIF_SOI       0xD8
#define USBH_UVC_JFIF_EOI       0xD9

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPD)
#define USBH_UVC_HTTPD_TX_SIZE  4000
#if (CONFIG_USBH_UVC_USE_HTTPS == 0)
#define USBH_UVC_HTTPD_SECURE   HTTPD_SECURE_NONE
#define USBH_UVC_HTTPD_PORT     80
#else // (CONFIG_USBH_UVC_USE_HTTPS == 1)
#define USBH_UVC_HTTPD_SECURE   HTTPD_SECURE_TLS
#define USBH_UVC_HTTPD_PORT     443
#endif
#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPC)
#define USBH_UVC_HTTPC_TX_SIZE  4000
#define USBH_UVC_HTTPC_SERVER   "192.168.1.100"
#if (CONFIG_USBH_UVC_USE_HTTPS == 0)
#define USBH_UVC_HTTPC_SECURE   HTTPC_SECURE_NONE
#define USBH_UVC_HTTPC_PORT     80
#else // (CONFIG_USBH_UVC_USE_HTTPS == 1)
#define USBH_UVC_HTTPC_SECURE   HTTPC_SECURE_TLS
#define USBH_UVC_HTTPC_PORT     443
#endif
#endif

#if (CONFIG_USBH_UVC_USE_HTTPS == 0)
#define USBH_UVC_HTTP_TAG       "HTTP"
#else
#define USBH_UVC_HTTP_TAG       "HTTPS"
#endif

#define USBH_UVC_LOG(...)  printf(__VA_ARGS__)

#if (CONFIG_USBH_UVC_APP != USBH_UVC_APP_MMF_RTSP)
static _mutex uvc_buf_mutex = NULL;
PSRAM_BSS_SECTION u8 uvc_buf[USBH_UVC_BUF_SIZE];
static int uvc_buf_size = 0;
#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPC)
static _sema uvc_httpc_save_img_sema = NULL;
static int uvc_httpc_img_file_no = 0;
#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_FATFS)
static _sema uvc_fatfs_save_img_sema = NULL;
static int uvc_fatfs_is_init = 0;
static int uvc_fatfs_img_file_no = 0;
#endif

#if (CONFIG_USBH_UVC_APP != USBH_UVC_APP_MMF_RTSP)

static void uvc_img_prepare(struct uvc_buf_context *buf)
{
    u32 len = 0;
    u32 start = 0;

    if (buf != NULL) {
#if ((CONFIG_USBH_UVC_CHECK_IMAGE_DATA == 1) && (USBH_UVC_FORMAT_TYPE == 0))
        u8 *ptr = (u8 *)buf->data;
        len = buf->len;
        u32 end = buf->len - 2;

        if(len < 4){
            USBH_UVC_LOG("\nToo short uvc data, len=%d\n", len);
        }
        
        // Check mjpeg image data
        // Normally, the mjpeg image data shall start with SOI and end with EOI
        while(start < len-1){
            if ((ptr[start] == USBH_UVC_JFIF_TAG) && (ptr[start + 1] == USBH_UVC_JFIF_SOI)) { // Check SOI
                while(end > 0){
                    if(ptr[end + 1] != 0){               //some cameras have padding at the end
                        if((ptr[end] == USBH_UVC_JFIF_TAG) && (ptr[end + 1] == USBH_UVC_JFIF_EOI)){   //Check EOI
                            break;
                        }else{
                            USBH_UVC_LOG("\nInvalid uvc data, len=%d, end with %02X %02X\n", buf->len, ptr[end], ptr[end + 1]);
                            return;
                        }
                    }
                    end--;
                }
                break;
            }
            start++;
        }

        if(start == len-1){
            USBH_UVC_LOG("\nInvalid uvc data, len=%d, start with %02X %02X\n", buf->len, buf->data[0], buf->data[1]);
            return;
        }

         len = end + 2 - start;

        if (len == buf->len) {
            USBH_UVC_LOG("\nCapture uvc data, len=%d\n", len);
        } else {
            USBH_UVC_LOG("\nCapture uvc data, start=%d, end=%d, actul_len=%d, buf->len=%d\n", start, end+2, len, buf->len);
        }

#else
        len = buf->len;
#endif
        if(len > USBH_UVC_BUF_SIZE) {
            USBH_UVC_LOG("\nImage len overflow!\n");	
            return;
        }
        rtw_mutex_get(&uvc_buf_mutex);
        rtw_memcpy(uvc_buf, (void *)(buf->data + start), len);
        uvc_buf_size = len;
        rtw_mutex_put(&uvc_buf_mutex);

        if (uvc_buf_size > 0) {
#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_FATFS)
            if (uvc_fatfs_is_init) {
                rtw_up_sema(&uvc_fatfs_save_img_sema);
            }
#endif
#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPC)
            rtw_up_sema(&uvc_httpc_save_img_sema);
#endif
        }
    }
}

#endif // (CONFIG_USBH_UVC_APP != USBH_UVC_APP_MMF_RTSP)

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPD)

static void uvc_httpd_img_get_cb(struct httpd_conn *conn)
{
    u32 send_offset = 0;
    int left_size;
    int ret;
    
    httpd_conn_dump_header(conn);
    
    // GET img page
    ret = httpd_request_is_method(conn, "GET");
    if (ret) {
        rtw_mutex_get(&uvc_buf_mutex);
        left_size = uvc_buf_size;
        httpd_response_write_header_start(conn, "200 OK", "image/jpeg", uvc_buf_size);
        httpd_response_write_header(conn, "connection", "close");
        httpd_response_write_header_finish(conn);

        while (left_size > 0) {
            if (left_size > USBH_UVC_HTTPD_TX_SIZE) {
                ret = httpd_response_write_data(conn, uvc_buf + send_offset, USBH_UVC_HTTPD_TX_SIZE);
                if (ret < 0) {
                    USBH_UVC_LOG("\nFail to send %s response: %d\n", USBH_UVC_HTTP_TAG, ret);
                    break;
                }

                send_offset += USBH_UVC_HTTPD_TX_SIZE;
                left_size -= USBH_UVC_HTTPD_TX_SIZE;
            } else {
                ret = httpd_response_write_data(conn, uvc_buf + send_offset, left_size);
                if (ret < 0) {
                    USBH_UVC_LOG("\nFail to send %s response: %d\n", USBH_UVC_HTTP_TAG, ret);
                    break;
                }

                USBH_UVC_LOG("\nUVC image sent, %d bytes\n", uvc_buf_size);
                left_size = 0;
            }

            rtw_mdelay_os(1);
        }

        rtw_mutex_put(&uvc_buf_mutex);
    } else {
        httpd_response_method_not_allowed(conn, NULL);
    }

    httpd_conn_close(conn);
}

static int uvc_httpd_start(void)
{
    int ret;
    
#if CONFIG_USBH_UVC_USE_HTTPS
#if (HTTPD_USE_TLS == HTTPD_TLS_POLARSSL)
    ret = httpd_setup_cert(test_srv_crt, test_srv_key, test_ca_crt);
#elif (HTTPD_USE_TLS == HTTPD_TLS_MBEDTLS)
    ret = httpd_setup_cert(mbedtls_test_srv_crt, mbedtls_test_srv_key, mbedtls_test_ca_crt);
#endif
    if (ret != 0) {
        USBH_UVC_LOG("\nFail to setup %s server cert\n", USBH_UVC_HTTP_TAG);
        return ret;
    }

#endif
    httpd_reg_page_callback("/", uvc_httpd_img_get_cb);
    ret = httpd_start(USBH_UVC_HTTPD_PORT, 2, 4096, HTTPD_THREAD_SINGLE, USBH_UVC_HTTPD_SECURE);
    if (ret != 0) {
        USBH_UVC_LOG("\nFail to start %s server: %d\n", USBH_UVC_HTTP_TAG, ret);
        httpd_clear_page_callbacks();
    }

    return ret;
}

#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPC)

static void uvc_httpc_thread(void *param)
{
    u32 send_offset;
    int left_size;
    int ret;
    char img_file[32];
    struct httpc_conn *conn = NULL;
    
    UNUSED(param);
    
    rtw_mdelay_os(5000); // Wait for network ready

    while (1) {
        rtw_down_sema(&uvc_httpc_save_img_sema);
        conn = httpc_conn_new(USBH_UVC_HTTPC_SECURE, NULL, NULL, NULL);
        if (conn) {
            ret = httpc_conn_connect(conn, USBH_UVC_HTTPC_SERVER, USBH_UVC_HTTPC_PORT, 0);
            if (ret == 0) {
                rtw_mutex_get(&uvc_buf_mutex);
                sprintf(img_file, "/uploads/img%d.jpeg", uvc_httpc_img_file_no);
                // start a header and add Host (added automatically), Content-Type and Content-Length (added by input param)
                httpc_request_write_header_start(conn, "PUT", img_file, NULL, uvc_buf_size);
                // add other header fields if necessary
                httpc_request_write_header(conn, "Connection", "close");
                // finish and send header
                httpc_request_write_header_finish(conn);
                send_offset = 0;
                left_size = uvc_buf_size;

                // send http body
                while (left_size > 0) {
                    if (left_size > USBH_UVC_HTTPC_TX_SIZE) {
                        ret = httpc_request_write_data(conn, uvc_buf + send_offset, USBH_UVC_HTTPC_TX_SIZE);
                        if (ret < 0) {
                            USBH_UVC_LOG("\nFail to send %s request: %d\n", USBH_UVC_HTTP_TAG, ret);
                            break;
                        }

                        send_offset += USBH_UVC_HTTPC_TX_SIZE;
                        left_size -= USBH_UVC_HTTPC_TX_SIZE;
                    } else {
                        ret = httpc_request_write_data(conn, uvc_buf + send_offset, left_size);
                        if (ret < 0) {
                            USBH_UVC_LOG("\nFail to send %s request: %d\n", USBH_UVC_HTTP_TAG, ret);
                            break;
                        }

                        USBH_UVC_LOG("\nUVC image img%d.jpeg sent, %d bytes\n", uvc_httpc_img_file_no, uvc_buf_size);
                        left_size = 0;
                    }

                    rtw_mdelay_os(1);
                }

                rtw_mutex_put(&uvc_buf_mutex);
                // receive response header
                ret = httpc_response_read_header(conn);
                if (ret == 0) {
                    httpc_conn_dump_header(conn);
                    // receive response body
                    ret = httpc_response_is_status(conn, "200 OK");
                    if (ret) {
                        uint8_t buf[1024];
                        int read_size = 0;
                        uint32_t total_size = 0;

                        while (1) {
                            rtw_memset(buf, 0, sizeof(buf));
                            read_size = httpc_response_read_data(conn, buf, sizeof(buf) - 1);
                            if (read_size > 0) {
                                total_size += read_size;
                                USBH_UVC_LOG("\n%s\n", buf);
                            } else {
                                break;
                            }

                            if (conn->response.content_len && (total_size >= conn->response.content_len)) {
                                break;
                            }
                        }
                    }
                }

                uvc_httpc_img_file_no++;
            } else {
                USBH_UVC_LOG("\nFail to connect to %s server\n", USBH_UVC_HTTP_TAG);
            }

            httpc_conn_close(conn);
            httpc_conn_free(conn);
        }
    }

    rtw_thread_exit();
}

static int uvc_httpc_start(void)
{
    int ret = 0;
    struct task_struct task;
    
    rtw_init_sema(&uvc_httpc_save_img_sema, 0);
    
    ret = rtw_create_task(&task, "uvc_httpc_thread", 2048, tskIDLE_PRIORITY + 2, uvc_httpc_thread, NULL);
    if (ret != pdPASS) {
        USBH_UVC_LOG("\n%Fail to create USB host UVC %s client thread\n", USBH_UVC_HTTP_TAG);
        rtw_free_sema(&uvc_httpc_save_img_sema);
        ret = 1;
    } else {
        ret = 0;
    }

    return ret;
}

#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_FATFS)

static void uvc_fatfs_thread(void *param)
{
    int drv_num = 0;
    int fatfs_ok = 0;
    FRESULT res;
    FATFS m_fs;
    FIL m_file;
    char logical_drv[4];
    char f_num[15];
    char filename[64] = {0};
    char path[64];
    int bw;
    int ret;
    
    UNUSED(param);
    
    // Register disk driver to fatfs
    drv_num = FATFS_RegisterDiskDriver(&SD_disk_Driver);
    if (drv_num < 0) {
        USBH_UVC_LOG("\nFail to register SD disk driver to FATFS\n");
    } else {
        fatfs_ok = 1;
        logical_drv[0] = drv_num + '0';
        logical_drv[1] = ':';
        logical_drv[2] = '/';
        logical_drv[3] = 0;
    }

    // Save image file to disk
    if (fatfs_ok) {
        res = f_mount(&m_fs, logical_drv, 1);
        if (res != FR_OK) {
            USBH_UVC_LOG("\nFail to mount logical drive: %d\n", res);
            goto fail;
        }

        rtw_init_sema(&uvc_fatfs_save_img_sema, 0);
        uvc_fatfs_is_init = 1;

        while (uvc_fatfs_is_init) {
            rtw_down_sema(&uvc_fatfs_save_img_sema);
            
            memset(filename, 0, 64);
            sprintf(filename, "img");
            sprintf(f_num, "%d", uvc_fatfs_img_file_no);
            strcat(filename, f_num);
            strcat(filename, ".jpeg");
            strcpy(path, logical_drv);
            sprintf(&path[strlen(path)], "%s", filename);
            
            res = f_open(&m_file, path, FA_OPEN_ALWAYS | FA_READ | FA_WRITE);
            if (res) {
                USBH_UVC_LOG("\nFail to open file (%s): %d\n", filename, res);
                continue;
            }

            USBH_UVC_LOG("\nCreate image file: %s\n", filename);

            if ((&m_file)->fsize > 0) {
                res = f_lseek(&m_file, 0);
                if (res) {
                    f_close(&m_file);
                    USBH_UVC_LOG("\nFail to seek file: %d\n", res);
                    goto out;
                }
            }

            rtw_mutex_get(&uvc_buf_mutex);

            do {
                res = f_write(&m_file, uvc_buf, uvc_buf_size, (u32 *)&bw);
                if (res) {
                    f_lseek(&m_file, 0);
                    USBH_UVC_LOG("\nFail to write file: %d\n", res);
                    break;
                }

                USBH_UVC_LOG("\nWrite %d bytes\n", bw);
            } while (bw < uvc_buf_size);

            rtw_mutex_put(&uvc_buf_mutex);
            
            res = f_close(&m_file);
            if (res) {
                USBH_UVC_LOG("\nFail to close file (%s): d\n", filename, res);
            }

            uvc_fatfs_img_file_no++;
        }

out:
        rtw_free_sema(&uvc_fatfs_save_img_sema);
        
        res = f_mount(NULL, logical_drv, 1);
        if (res != FR_OK) {
            USBH_UVC_LOG("\nFail to unmount logical drive: %d\n", res);
        }

        ret = FATFS_UnRegisterDiskDriver(drv_num);
        if (ret) {
            USBH_UVC_LOG("\nFail to unregister disk driver from FATFS: %d\n", ret);
        }
    }

fail:
    rtw_thread_exit();
}

static int uvc_fatfs_start(void)
{
    int ret;
    struct task_struct task;
    
    ret = rtw_create_task(&task, "uvc_fatfs_thread", 1024, tskIDLE_PRIORITY + 2, uvc_fatfs_thread, NULL);
    if (ret != pdPASS) {
        USBH_UVC_LOG("\n%Fail to create USB host UVC fatfs thread\n");
        ret = 1;
    } else {
        ret = 0;
    }

    return ret;
}

#endif

#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_MMF_RTSP)

static uvc_parameter_t uvc_params = {
    .fmt_type          = USBH_UVC_FORMAT_TYPE,
    .width             = USBH_UVC_WIDTH,
    .height            = USBH_UVC_HEIGHT,
    .frame_rate        = USBH_UVC_FRAME_RATE,
    .compression_ratio = USBH_UVC_COMP_RATIO,
    .frame_buf_size    = USBH_UVC_BUF_SIZE,
    .ext_frame_buf     = 1,
#if UVC_BUF_DYNAMIC
    .req_buf_num       = USBH_UVC_REQBUF_NUM,
    .req_buf_size      = USBH_UVC_REQBUF_SIZE
#endif
};

static rtsp2_params_t rtsp2_params = {
    .type = AVMEDIA_TYPE_VIDEO,
    .u = {
        .v = {
            .codec_id = AV_CODEC_ID_MJPEG,
            .fps      = USBH_UVC_FRAME_RATE,
        }
    }
};

static void example_usbh_uvc_task(void *param)
{
    int ret = 0;
    UNUSED(param);
    mm_context_t *uvch_ctx = NULL;
    mm_context_t *rtsp_ctx = NULL;
    mm_siso_t *uvc_to_rstp_siso = NULL;
    
    rtw_mdelay_os(5000); // Wait for network ready
    
    USBH_UVC_LOG("\nOpen MMF UVCH module\n");
    
    uvch_ctx = mm_module_open(&uvch_module);
    if (uvch_ctx != NULL) {
        ret = mm_module_ctrl(uvch_ctx, MM_CMD_SET_QUEUE_LEN, 1);
        if (ret != 0) {
            USBH_UVC_LOG("\nMM_CMD_SET_QUEUE_LEN failed: %d\n", ret);
            goto usbh_uvc_task_exit;
        }
        ret = mm_module_ctrl(uvch_ctx, MM_CMD_UVCH_SET_PARAMS, (int)&uvc_params);
        if (ret != 0) {
            USBH_UVC_LOG("\nMM_CMD_UVCH_SET_PARAMS failed: %d\n", ret);
            goto usbh_uvc_task_exit;
        }
        ret = mm_module_ctrl(uvch_ctx, MM_CMD_UVCH_APPLY, 0);
        if (ret != 0) {
            USBH_UVC_LOG("\nMM_CMD_UVCH_APPLY failed: %d\n", ret);
            goto usbh_uvc_task_exit;
        }
        ret = mm_module_ctrl(uvch_ctx, MM_CMD_UVCH_SET_STREAMING, ON);
        if (ret != 0) {
            USBH_UVC_LOG("\nMM_CMD_UVCH_SET_STREAMING ON failed: %d\n", ret);
            goto usbh_uvc_task_uvch_free;
        }
    } else {
        ret = -1;
        USBH_UVC_LOG("\nFail to open MMF UVCH module\n");
        goto usbh_uvc_task_exit;
    }

    USBH_UVC_LOG("\nOpen MMF RTSP module\n");
    
    rtsp_ctx = mm_module_open(&rtsp2_module);
    if (rtsp_ctx != NULL) {
        ret = mm_module_ctrl(rtsp_ctx, CMD_RTSP2_SELECT_STREAM, 0);
        if (ret != 0) {
            USBH_UVC_LOG("\nCMD_RTSP2_SELECT_STREAM failed: %d\n", ret);
            goto usbh_uvc_task_uvch_free;
        }
        ret = mm_module_ctrl(rtsp_ctx, CMD_RTSP2_SET_PARAMS, (int)&rtsp2_params);
        if (ret != 0) {
            USBH_UVC_LOG("\nCMD_RTSP2_SET_PARAMS failed: %d\n", ret);
            goto usbh_uvc_task_uvch_free;
        }
        ret = mm_module_ctrl(rtsp_ctx, CMD_RTSP2_SET_APPLY, 0);
        if (ret != 0) {
            USBH_UVC_LOG("\nCMD_RTSP2_SET_APPLY failed: %d\n", ret);
            goto usbh_uvc_task_uvch_free;
        }
        ret = mm_module_ctrl(rtsp_ctx, CMD_RTSP2_SET_STREAMMING, ON);
        if (ret != 0) {
            USBH_UVC_LOG("\nCMD_RTSP2_SET_STREAMMING ON failed: %d\n", ret);
            goto usbh_uvc_task_rtsp_free;
        }
    } else {
        ret = -1;
        USBH_UVC_LOG("\nFail to open MMF RTSP module\n");
        goto usbh_uvc_task_exit;
    }

    USBH_UVC_LOG("\nCreate MMF UVCH-RSTP SISO\n");
    
    uvc_to_rstp_siso = siso_create();
    if (uvc_to_rstp_siso != NULL) {
        siso_ctrl(uvc_to_rstp_siso, MMIC_CMD_ADD_INPUT, (uint32_t)uvch_ctx, 0);
        siso_ctrl(uvc_to_rstp_siso, MMIC_CMD_ADD_OUTPUT, (uint32_t)rtsp_ctx, 0);
        ret = siso_start(uvc_to_rstp_siso);
        if (ret == 0) {
            goto usbh_uvc_task_exit;
        } else {
            USBH_UVC_LOG("\nFail to start MMF UVCH-RSTP SISO: %d\n", ret);
        }
    } else {
        ret = -1;
        USBH_UVC_LOG("\nFail to create MMF UVCH-RSTP SISO\n");
    }

usbh_uvc_task_rtsp_free:
    mm_module_close(rtsp_ctx);

usbh_uvc_task_uvch_free:
    mm_module_close(uvch_ctx);    

usbh_uvc_task_exit:     
    if (ret == 0) {
        USBH_UVC_LOG("\nMMF UVCH-RSTP started\n");
    } else {
        USBH_UVC_LOG("\nMMF UVCH-RSTP aborted\n");
    }

    rtw_thread_exit();
}

#elif (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPD)
static _sema UvcSema;

static void uvc_attach(void)
{
	rtw_up_sema(&UvcSema);
}

struct uvc_usr_cb_t uvc_usr_cb = {
	.attach = uvc_attach,
};
static void example_usbh_uvc_task(void *param)
{
	int ret = 0;
	struct uvc_buf_context buf;
	struct uvc_context *uvc_ctx;
	int uvc_service_started = 0;
	UNUSED(param);

	USBH_UVC_LOG("\nInit USB host driver\n");
	ret = usb_init();
	if (ret != USB_INIT_OK) {
		USBH_UVC_LOG("\nFail to init USB\n");
		goto exit;
	}

	rtw_init_sema(&UvcSema, 0);
	uvc_register_usr_cb(&uvc_usr_cb);

	USBH_UVC_LOG("\nInit UVC driver\n");
#if UVC_BUF_DYNAMIC
	ret = uvc_stream_init(USBH_UVC_REQBUF_NUM, USBH_UVC_REQBUF_SIZE);
#else
	ret = uvc_stream_init();
#endif
	if (ret < 0) {
		USBH_UVC_LOG("\nFail to init UVC driver\n");
		goto exit1;
	}

	USBH_UVC_LOG("\nSet UVC parameters\n");
	uvc_ctx = (struct uvc_context *)rtw_malloc(sizeof(struct uvc_context));
	if (!uvc_ctx) {
		USBH_UVC_LOG("\nFail to allocate UVC context\n");
		goto exit1;
	}

	uvc_ctx->fmt_type = USBH_UVC_FORMAT_TYPE;
	uvc_ctx->width = USBH_UVC_WIDTH;
	uvc_ctx->height = USBH_UVC_HEIGHT;
	uvc_ctx->frame_rate = USBH_UVC_FRAME_RATE;
	uvc_ctx->compression_ratio = USBH_UVC_COMP_RATIO;

	rtw_mutex_init(&uvc_buf_mutex);
	
	while(1) {
		rtw_down_sema(&UvcSema);
		USBH_UVC_LOG("\nSet UVC parameters\n");
		ret = uvc_set_param(uvc_ctx->fmt_type, &uvc_ctx->width, &uvc_ctx->height, &uvc_ctx->frame_rate,
		&uvc_ctx->compression_ratio);
		if (ret < 0) {
			USBH_UVC_LOG("\nFail to set UVC parameters: %d\n", ret);
			goto exit1;
		}

		ret = uvc_stream_on();
		if (ret < 0) {
			USBH_UVC_LOG("\nFail to turn on UVC stream: %d\n", ret);
			continue;
		}

		if (!uvc_service_started) {
			USBH_UVC_LOG("\nStart %s server\n", USBH_UVC_HTTP_TAG);
			ret = uvc_httpd_start();
			if (ret != 0) {
				USBH_UVC_LOG("\nFail to start %s server: %d\n", USBH_UVC_HTTP_TAG, ret);
				continue;
			}
			uvc_service_started = 1;
		}
	
		USBH_UVC_LOG("\nStart capturing images\n");

		while (1) {
			if(!usbh_get_connect_status()) {
				break;
			}
			ret = uvc_dqbuf(&buf);

			if (buf.index < 0) {
				USBH_UVC_LOG("\nFail to dequeue UVC buffer\n");
				ret = 1;
			} else if ((uvc_buf_check(&buf) < 0) || (ret < 0)) {
				USBH_UVC_LOG("\nUVC buffer error: %d\n", ret);
				uvc_stream_off();
				break;
			}

			if (ret == 0) {
				uvc_img_prepare(&buf);
			}

			ret = uvc_qbuf(&buf);
			if (ret < 0) {
				USBH_UVC_LOG("\nFail to queue UVC buffer\n");
			}

			rtw_mdelay_os(1000);
		}

		USBH_UVC_LOG("\nStop capturing images\n");
	}
exit2:
	rtw_mutex_free(&uvc_buf_mutex);
	uvc_stream_free();
	rtw_free(uvc_ctx);
exit1:
	rtw_free_sema(&UvcSema);
	usb_deinit();
exit:
	rtw_thread_exit();
}

#else

static _sema UvcSema;
static _sema UvcDisconnSema;

static void uvc_attach(void)
{
	rtw_up_sema(&UvcSema);
}

static void uvc_detach(void)
{
	rtw_down_sema(&UvcDisconnSema);
}

struct uvc_usr_cb_t uvc_usr_cb = {
	.attach = uvc_attach,
	.detach = uvc_detach,
};
static void example_usbh_uvc_task(void *param)
{
	int ret = 0;
	struct uvc_buf_context buf;
	struct uvc_context *uvc_ctx;
	int uvc_service_started = 0;
	UNUSED(param);

	USBH_UVC_LOG("\nInit USB host driver\n");
	ret = usb_init();
	if (ret != USB_INIT_OK) {
		USBH_UVC_LOG("\nFail to init USB\n");
		goto exit;
	}

	rtw_init_sema(&UvcSema, 0);
	rtw_init_sema(&UvcDisconnSema, 0);
	uvc_register_usr_cb(&uvc_usr_cb);

	USBH_UVC_LOG("\nInit UVC driver\n");
#if UVC_BUF_DYNAMIC
	ret = uvc_stream_init(USBH_UVC_REQBUF_NUM, USBH_UVC_REQBUF_SIZE);
#else
	ret = uvc_stream_init();
#endif
	if (ret < 0) {
		USBH_UVC_LOG("\nFail to init UVC driver\n");
		goto exit1;
	}

	uvc_ctx = (struct uvc_context *)rtw_malloc(sizeof(struct uvc_context));
	if (!uvc_ctx) {
		USBH_UVC_LOG("\nFail to allocate UVC context\n");
		goto exit1;
	}

	uvc_ctx->fmt_type = USBH_UVC_FORMAT_TYPE;
	uvc_ctx->width = USBH_UVC_WIDTH;
	uvc_ctx->height = USBH_UVC_HEIGHT;
	uvc_ctx->frame_rate = USBH_UVC_FRAME_RATE;
	uvc_ctx->compression_ratio = USBH_UVC_COMP_RATIO;

	rtw_mutex_init(&uvc_buf_mutex);

	while(1) {
		rtw_down_sema(&UvcSema);
		USBH_UVC_LOG("\nSet UVC parameters\n");
		ret = uvc_set_param(uvc_ctx->fmt_type, &uvc_ctx->width, &uvc_ctx->height, &uvc_ctx->frame_rate,
		&uvc_ctx->compression_ratio);
		if (ret < 0) {
			USBH_UVC_LOG("\nFail to set UVC parameters: %d\n", ret);
			goto exit1;
		}

		if (!uvc_service_started) {
		#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_HTTPC)
			USBH_UVC_LOG("\nStart %s client\n", USBH_UVC_HTTP_TAG);
			ret = uvc_httpc_start();
			if (ret != 0) {
				USBH_UVC_LOG("\nFail to start %s client: %d\n", USBH_UVC_HTTP_TAG, ret);
				continue;
			}
		#endif

		#if (CONFIG_USBH_UVC_APP == USBH_UVC_APP_FATFS)
			USBH_UVC_LOG("\nStart FATFS service\n");
			ret = uvc_fatfs_start();
			if (ret != 0) {
				USBH_UVC_LOG("\nFail to start fatfs: %d\n", ret);
				continue;
			}
		#endif
			uvc_service_started = 1;
		}
		
		while(1) {
			if(!usbh_get_connect_status()) {
				rtw_up_sema(&UvcDisconnSema);
				break;
			}
			
			ret = uvc_stream_on();
			if (ret < 0) {
				USBH_UVC_LOG("\nFail to turn on UVC stream: %d\n", ret);
				continue;
			}

			if(!usbh_get_connect_status()) {
				rtw_up_sema(&UvcDisconnSema);
				break;
			}
			USBH_UVC_LOG("\nStart capturing images\n");		
			ret = uvc_dqbuf(&buf);
			if (buf.index < 0) {
				USBH_UVC_LOG("\nFail to dequeue UVC buffer\n");
				ret = 1;
			} else if ((uvc_buf_check(&buf) < 0) || (ret < 0)) {
				USBH_UVC_LOG("\nUVC buffer error: %d\n", ret);
				uvc_stream_off();
				continue;
			}

			if(!usbh_get_connect_status()) {
				rtw_up_sema(&UvcDisconnSema);
				break;
			}
			
			if (ret == 0) {
				uvc_img_prepare(&buf);
			}
			
			USBH_UVC_LOG("\nStop capturing images\n");			
			uvc_stream_off();
			rtw_mdelay_os(1000);
		}
	}

exit2:
	rtw_mutex_free(&uvc_buf_mutex);
	uvc_stream_free();
	rtw_free(uvc_ctx);
exit1:
	rtw_free_sema(&UvcSema);
	rtw_free_sema(&UvcDisconnSema);
	usb_deinit();
exit:
	rtw_thread_exit();
}

#endif // (CONFIG_USBH_UVC_APP == USBH_UVC_APP_MMF_RTSP)

void example_usbh_uvc(void)
{
    int ret;
    struct task_struct task;
    
    USBH_UVC_LOG("\nUSB host UVC demo started...\n");
    
    ret = rtw_create_task(&task, "example_usbh_uvc_task", 1024, tskIDLE_PRIORITY + 1, example_usbh_uvc_task, NULL);
    if (ret != pdPASS) {
        USBH_UVC_LOG("\nFail to create USB host UVC thread\n");
    }
}

#endif
