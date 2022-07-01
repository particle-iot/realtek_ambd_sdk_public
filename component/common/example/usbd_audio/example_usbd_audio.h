#ifndef __EXAMPLE_USBD_AUDIO_H__
#define __EXAMPLE_USBD_AUDIO_H__

#include "ameba_soc.h"

#define USB_AUDIO_BUF_SIZE  576
#define SP_DMA_PAGE_SIZE    USB_AUDIO_BUF_SIZE
#define SP_FULL_BUF_SIZE    128
#define SP_DMA_PAGE_NUM     4
#define SP_ZERO_BUF_SIZE    128
#define PS_USED 0
#define RECV_BUF_NUM        4
#define HEADPHONE_MIC       1

typedef struct {
    GDMA_InitTypeDef        SpTxGdmaInitStruct;              //Pointer to GDMA_InitTypeDef
    GDMA_InitTypeDef        SpRxGdmaInitStruct;              //Pointer to GDMA_InitTypeDef
} SP_GDMA_STRUCT, *pSP_GDMA_STRUCT;

typedef struct {
    u8 tx_gdma_own;
    u32 tx_addr;
    u32 tx_length;

} TX_BLOCK, *pTX_BLOCK;

typedef struct {
    TX_BLOCK tx_block[SP_DMA_PAGE_NUM];
    TX_BLOCK tx_zero_block;
    u8 tx_gdma_cnt;
    u8 tx_usr_cnt;
    u8 tx_empty_flag;

} SP_TX_INFO, *pSP_TX_INFO;

typedef struct {
    u8 rx_gdma_own;
    u32 rx_addr;
    u32 rx_length;

} RX_BLOCK, *pRX_BLOCK;

typedef struct {
    RX_BLOCK rx_block[SP_DMA_PAGE_NUM];
    RX_BLOCK rx_full_block;
    u8 rx_gdma_cnt;
    u8 rx_usr_cnt;
    u8 rx_full_flag;

} SP_RX_INFO, *pSP_RX_INFO;
typedef struct {
    u32 sample_rate;
    u32 word_len;
    u32 mono_stereo;
    u32 direction;

} SP_OBJ, *pSP_OBJ;

typedef struct _recv_pkt_t {
    uint8_t *data;
    uint32_t data_len;
} recv_pkt_t;

extern u8 *sp_get_free_rx_page(void);
extern u8 *sp_get_free_tx_page(void);
extern u32 sp_get_free_rx_length(void);
extern void sp_write_tx_page(u8 *src, u32 length);
extern void sp_release_tx_page(void);
extern u8 *sp_get_ready_tx_page(void);
extern u32 sp_get_ready_tx_length(void);
extern void sp_tx_complete(void *Data);
extern void sp_init_hal(pSP_OBJ psp_obj);
extern void sp_rx_complete(void *data);
extern void sp_init_rx_variables(void);
extern void sp_init_tx_variables(void);

void example_usbd_audio(void);

#endif
