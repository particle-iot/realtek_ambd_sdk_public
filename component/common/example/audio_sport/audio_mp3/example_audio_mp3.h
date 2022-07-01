#ifndef _EXAMPLE_AUDIO_MP3_H_
#define _EXAMPLE_AUDIO_MP3_H_

#include "ameba_soc.h"

#define MP3_DECODE_SIZE		4608
#define SP_DMA_PAGE_SIZE		MP3_DECODE_SIZE/2   // 2 ~ 4096
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

void example_audio_mp3_thread(void* param);

#endif
