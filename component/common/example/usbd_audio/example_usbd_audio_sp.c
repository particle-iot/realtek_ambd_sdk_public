#include <platform_opts.h>
#if defined(CONFIG_EXAMPLE_USBD_AUDIO) && CONFIG_EXAMPLE_USBD_AUDIO
#include <platform/platform_stdlib.h>
#include "usbd_audio/example_usbd_audio.h"
#include "basic_types.h"
#include "rl6548.h"
#include "osdep_service.h"

static SP_RX_INFO sp_rx_info;
static SP_TX_INFO sp_tx_info;
static u8 sp_tx_buf[SP_DMA_PAGE_SIZE * SP_DMA_PAGE_NUM];
static u8 sp_zero_buf[SP_ZERO_BUF_SIZE];
SRAM_NOCACHE_DATA_SECTION
static u8 sp_rx_buf[SP_DMA_PAGE_SIZE * SP_DMA_PAGE_NUM];
static u8 sp_full_buf[SP_FULL_BUF_SIZE];

extern void PLL_Div(u32 div);

u8 *sp_get_ready_rx_page(void)
{
    pRX_BLOCK prx_block = &(sp_rx_info.rx_block[sp_rx_info.rx_usr_cnt]);

    if (prx_block->rx_gdma_own) {
        return (u8 *)NULL;
    } else {
        return (u8 *)prx_block->rx_addr;
    }
}

void sp_read_rx_page(u8 *dst, u32 length)
{
    pRX_BLOCK prx_block = &(sp_rx_info.rx_block[sp_rx_info.rx_usr_cnt]);
    rtw_memcpy((void *)dst, (void *)prx_block->rx_addr, length);
    prx_block->rx_gdma_own = 1;
    sp_rx_info.rx_usr_cnt++;

    if (sp_rx_info.rx_usr_cnt == SP_DMA_PAGE_NUM) {
        sp_rx_info.rx_usr_cnt = 0;
    }
}
void sp_release_rx_page(void)
{
    pRX_BLOCK prx_block = &(sp_rx_info.rx_block[sp_rx_info.rx_gdma_cnt]);

    if (sp_rx_info.rx_full_flag) {
    } else {
        prx_block->rx_gdma_own = 0;
        sp_rx_info.rx_gdma_cnt++;

        if (sp_rx_info.rx_gdma_cnt == SP_DMA_PAGE_NUM) {
            sp_rx_info.rx_gdma_cnt = 0;
        }
    }
}
u8 *sp_get_free_rx_page(void)
{
    pRX_BLOCK prx_block = &(sp_rx_info.rx_block[sp_rx_info.rx_gdma_cnt]);

    if (prx_block->rx_gdma_own) {
        sp_rx_info.rx_full_flag = 0;
        return (u8 *)prx_block->rx_addr;
    } else {
        sp_rx_info.rx_full_flag = 1;
        return (u8 *)sp_rx_info.rx_full_block.rx_addr;  //for audio buffer full case
    }
}
u32 sp_get_free_rx_length(void)
{
    pRX_BLOCK prx_block = &(sp_rx_info.rx_block[sp_rx_info.rx_gdma_cnt]);

    if (sp_rx_info.rx_full_flag) {
        return sp_rx_info.rx_full_block.rx_length;
    } else {
        return prx_block->rx_length;
    }
}

void sp_init_rx_variables(void)
{
    int i;
    sp_rx_info.rx_full_block.rx_addr = (u32)sp_full_buf;
    sp_rx_info.rx_full_block.rx_length = (u32)SP_FULL_BUF_SIZE;
    sp_rx_info.rx_gdma_cnt = 0;
    sp_rx_info.rx_usr_cnt = 0;
    sp_rx_info.rx_full_flag = 0;

    for (i = 0; i < SP_DMA_PAGE_NUM; i++) {
        sp_rx_info.rx_block[i].rx_gdma_own = 1;
        sp_rx_info.rx_block[i].rx_addr = (u32)(sp_rx_buf + i * SP_DMA_PAGE_SIZE);
        sp_rx_info.rx_block[i].rx_length = SP_DMA_PAGE_SIZE;
    }
}
extern u8 mic_recv_pkt_buff[SP_DMA_PAGE_SIZE * RECV_BUF_NUM];

void sp_rx_complete(void *Data)
{
    SP_GDMA_STRUCT *gs = (SP_GDMA_STRUCT *) Data;
    PGDMA_InitTypeDef GDMA_InitStruct;
    u32 rx_addr;
    u32 rx_length;
    static int count = 0;
    GDMA_InitStruct = &(gs->SpRxGdmaInitStruct);
    /* Clear Pending ISR */
    GDMA_ClearINT(GDMA_InitStruct->GDMA_Index, GDMA_InitStruct->GDMA_ChNum);
    sp_release_rx_page();

    if (sp_get_ready_rx_page() != NULL) {
        sp_read_rx_page((u8 *)&mic_recv_pkt_buff[SP_DMA_PAGE_SIZE * count], SP_DMA_PAGE_SIZE);
        count++;
        count = count % RECV_BUF_NUM;
    }

    rx_addr = (u32)sp_get_free_rx_page();
    rx_length = sp_get_free_rx_length();
    GDMA_SetDstAddr(GDMA_InitStruct->GDMA_Index, GDMA_InitStruct->GDMA_ChNum, rx_addr);
    GDMA_SetBlkSize(GDMA_InitStruct->GDMA_Index, GDMA_InitStruct->GDMA_ChNum, rx_length >> 2);
    GDMA_Cmd(GDMA_InitStruct->GDMA_Index, GDMA_InitStruct->GDMA_ChNum, ENABLE);
}

u8 *sp_get_free_tx_page(void)
{
    pTX_BLOCK ptx_block = &(sp_tx_info.tx_block[sp_tx_info.tx_usr_cnt]);

    if (ptx_block->tx_gdma_own) {
        return (u8 *)NULL;
    } else {
        return (u8 *)ptx_block->tx_addr;
    }
}

void sp_write_tx_page(u8 *src, u32 length)
{
    pTX_BLOCK ptx_block = &(sp_tx_info.tx_block[sp_tx_info.tx_usr_cnt]);
    rtw_memcpy((void *)ptx_block->tx_addr, (void *)src, length);
    ptx_block->tx_length = length;
    ptx_block->tx_gdma_own = 1;
    sp_tx_info.tx_usr_cnt++;

    if (sp_tx_info.tx_usr_cnt == SP_DMA_PAGE_NUM) {
        sp_tx_info.tx_usr_cnt = 0;
    }
}

void sp_release_tx_page(void)
{
    pTX_BLOCK ptx_block = &(sp_tx_info.tx_block[sp_tx_info.tx_gdma_cnt]);

    if (sp_tx_info.tx_empty_flag) {
    } else {
        ptx_block->tx_gdma_own = 0;
        sp_tx_info.tx_gdma_cnt++;

        if (sp_tx_info.tx_gdma_cnt == SP_DMA_PAGE_NUM) {
            sp_tx_info.tx_gdma_cnt = 0;
        }
    }
}

u8 *sp_get_ready_tx_page(void)
{
    pTX_BLOCK ptx_block = &(sp_tx_info.tx_block[sp_tx_info.tx_gdma_cnt]);

    if (ptx_block->tx_gdma_own) {
        sp_tx_info.tx_empty_flag = 0;
        return (u8 *)ptx_block->tx_addr;
    } else {
        sp_tx_info.tx_empty_flag = 1;
        return (u8 *)sp_tx_info.tx_zero_block.tx_addr;  //for audio buffer empty case
    }
}

u32 sp_get_ready_tx_length(void)
{
    pTX_BLOCK ptx_block = &(sp_tx_info.tx_block[sp_tx_info.tx_gdma_cnt]);

    if (sp_tx_info.tx_empty_flag) {
        return sp_tx_info.tx_zero_block.tx_length;
    } else {
        return ptx_block->tx_length;
    }
}

void sp_init_tx_variables(void)
{
    int i;

    for (i = 0; i < SP_ZERO_BUF_SIZE; i++) {
        sp_zero_buf[i] = 0;
    }

    sp_tx_info.tx_zero_block.tx_addr = (u32)sp_zero_buf;
    sp_tx_info.tx_zero_block.tx_length = (u32)SP_ZERO_BUF_SIZE;
    sp_tx_info.tx_gdma_cnt = 0;
    sp_tx_info.tx_usr_cnt = 0;
    sp_tx_info.tx_empty_flag = 0;

    for (i = 0; i < SP_DMA_PAGE_NUM; i++) {
        sp_tx_info.tx_block[i].tx_gdma_own = 0;
        sp_tx_info.tx_block[i].tx_addr = (u32)(sp_tx_buf + i * SP_DMA_PAGE_SIZE);
        sp_tx_info.tx_block[i].tx_length = SP_DMA_PAGE_SIZE;
    }
}
void sp_tx_complete(void *Data)
{
    SP_GDMA_STRUCT *gs = (SP_GDMA_STRUCT *) Data;
    PGDMA_InitTypeDef GDMA_InitStruct;
    u32 tx_addr;
    u32 tx_length;
    GDMA_InitStruct = &(gs->SpTxGdmaInitStruct);
    /* Clear Pending ISR */
    GDMA_ClearINT(GDMA_InitStruct->GDMA_Index, GDMA_InitStruct->GDMA_ChNum);
    sp_release_tx_page();
    tx_addr = (u32)sp_get_ready_tx_page();
    tx_length = sp_get_ready_tx_length();
    GDMA_SetSrcAddr(GDMA_InitStruct->GDMA_Index, GDMA_InitStruct->GDMA_ChNum, tx_addr);
    GDMA_SetBlkSize(GDMA_InitStruct->GDMA_Index, GDMA_InitStruct->GDMA_ChNum, tx_length >> 2);
    GDMA_Cmd(GDMA_InitStruct->GDMA_Index, GDMA_InitStruct->GDMA_ChNum, ENABLE);
}

void sp_init_hal(pSP_OBJ psp_obj)
{
    u32 div;
    PLL_I2S_Set(ENABLE);        //enable 98.304MHz PLL. needed if fs=8k/16k/32k/48k/96k
    //PLL_PCM_Set(ENABLE);      //enable 45.1584MHz PLL. needed if fs=44.1k/8.2k
    RCC_PeriphClockCmd(APBPeriph_AUDIOC, APBPeriph_AUDIOC_CLOCK, ENABLE);
    RCC_PeriphClockCmd(APBPeriph_SPORT, APBPeriph_SPORT_CLOCK, ENABLE);

    //set clock divider to gen clock sample_rate*256 from 98.304M.
    switch (psp_obj->sample_rate) {
        case SR_48K:
            div = 8;
            break;

        case SR_96K:
            div = 4;
            break;

        case SR_32K:
            div = 12;
            break;

        case SR_16K:
            div = 24;
            break;

        case SR_8K:
            div = 48;
            break;

        default:
            DBG_8195A("sample rate not supported!!\n");
            break;
    }

    PLL_Div(div);
    /*codec init*/
    CODEC_Init(psp_obj->sample_rate, psp_obj->word_len, psp_obj->mono_stereo, psp_obj->direction);
    PAD_CMD(_PB_28, DISABLE);
    PAD_CMD(_PB_29, DISABLE);
    PAD_CMD(_PB_30, DISABLE);
    PAD_CMD(_PB_31, DISABLE);

    if ((psp_obj->direction & APP_DMIC_IN) == APP_DMIC_IN) {
        PAD_CMD(_PB_24, DISABLE);
        PAD_CMD(_PB_25, DISABLE);
        Pinmux_Config(_PB_24, PINMUX_FUNCTION_DMIC);
        Pinmux_Config(_PB_25, PINMUX_FUNCTION_DMIC);
    }
}

#endif
