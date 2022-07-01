#ifndef _EXAMPLE_AUDIO_EKHO_H_
#define _EXAMPLE_AUDIO_EKHO_H_

#include "ameba_soc.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define SP_DMA_PAGE_SIZE		640
#define SP_DMA_PAGE_NUM    		4
#define SP_ZERO_BUF_SIZE		128

typedef struct {
	GDMA_InitTypeDef       	SpTxGdmaInitStruct;              //Pointer to GDMA_InitTypeDef
	GDMA_InitTypeDef       	SpRxGdmaInitStruct;              //Pointer to GDMA_InitTypeDef		
}SP_GDMA_STRUCT, *pSP_GDMA_STRUCT;

typedef struct {
	u8 tx_gdma_own;
	u32 tx_addr;
	u32 tx_length;
	
}TX_BLOCK, *pTX_BLOCK;

typedef struct {
	TX_BLOCK tx_block[SP_DMA_PAGE_NUM];
	TX_BLOCK tx_zero_block;
	u8 tx_gdma_cnt;
	u8 tx_usr_cnt;
	u8 tx_empty_flag;
	
}SP_TX_INFO, *pSP_TX_INFO;

typedef struct {
	u32 sample_rate;
	u32 word_len;
	u32 mono_stereo;
	u32 direction;
	
}SP_OBJ, *pSP_OBJ;

static u8 *sp_get_free_tx_page(void);
static void sp_write_tx_page(u8 *src, u32 length);
void example_audio_ekho_thread(void* param);

#ifdef __cplusplus
}
#endif

#endif
