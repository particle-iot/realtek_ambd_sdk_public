
#ifndef _BSP_TOUCH_H_


#ifdef _BSP_TOUCH_C_
	#define BSP_TOUCH_EXT  
#else 
	#define BSP_TOUCH_EXT extern
#endif 

#include "platform_stdlib.h"
#include "basic_types.h"
#include "diag.h"
#include "ameba_soc.h"
#include "rtl8721d_lcdc.h"
#include "bsp_mcu_lcd.h"

//#define SDA_IN()    sda_in()//{GPIOG->CRL&=0X0FFFFFFF;GPIOG->CRL|=0X40000000;}	 //����ģʽ����������ģʽ
//#define SDA_OUT()   sda_out()//{GPIOG->CRL&=0X0FFFFFFF;GPIOG->CRL|=0X10000000;}	 //ͨ���������������ٶ�50MHZ

#define TP_PRES_DOWN 0x80  //����������	
#define TP_COORD_UD  0x40  //����������±��

//������������ݽṹ�嶨��
typedef struct			
{
	u8 TouchSta;	//���������b7:����1/�ɿ�0; b6:0û�а�������/1�а�������;bit5:������bit4-bit0�����㰴����Ч��־����ЧΪ1���ֱ��Ӧ������5-1��
	u16 x[5];		//֧��5�㴥������Ҫʹ��5������洢����������
	u16 y[5];
	
}TouchPointRefTypeDef;
extern TouchPointRefTypeDef TPR_Structure;

/*********************PIN DEFINITION*********************/	
#define RST_PIN 				_PA_18
#define SCL_PIN				_PB_5
#define SDA_PIN				_PB_6
#define INT_PIN				_PA_17


/*********************IO��������*********************/									
#define	FT6236_RST			RST_SET //PGout(6)	//�������ģʽ						
#define FT6236_SCL 			SCL_SET//PCout(7)	//�������ģʽ
#define FT6236_SDA 			SDA_SET//PGout(8)	//�������ģʽ
#define FT6236_SDA_Read  	GPIO_ReadDataBit(SDA_PIN)//PGin(8)		//��������ģʽ									
#define FT6236_INT 			INT_SET //PGin(6)		//��������ģʽ
							
//I2C��д����	
#define FT_CMD_WR 				0X70    	//д����
#define FT_CMD_RD 				0X71		//������
//FT6236 ���ּĴ������� 
#define FT_DEVIDE_MODE 			0x00   		//FT6236ģʽ���ƼĴ���
#define FT_REG_NUM_FINGER       0x02		//����״̬�Ĵ���

#define FT_TP1_REG 				0X03	  	//��һ�����������ݵ�ַ
#define FT_TP2_REG 				0X09		//�ڶ������������ݵ�ַ
#define FT_TP3_REG 				0X0F		//���������������ݵ�ַ
#define FT_TP4_REG 				0X15		//���ĸ����������ݵ�ַ
#define FT_TP5_REG 				0X1B		//��������������ݵ�ַ  
 

#define	FT_ID_G_LIB_VERSION		0xA1		//�汾		
#define FT_ID_G_MODE 			0xA4   		//FT6236�ж�ģʽ���ƼĴ���
#define FT_ID_G_THGROUP			0x80   		//������Чֵ���üĴ���
#define FT_ID_G_PERIODACTIVE	0x88   		//����״̬�������üĴ���  
#define Chip_Vendor_ID          0xA3        //оƬID(0x36)
#define ID_G_FT6236ID			0xA8		//0x11

u8 FT6236_WR_Reg(u16 reg,u8 *buf,u8 len);
void FT6236_RD_Reg(u16 reg,u8 *buf,u8 len);
void FT6236_Init(void);
void FT6236_Scan(void);
void sda_in(void);
void sda_out(void);

typedef enum
{
	TOUCH_PRESS = 1,
	TOUCH_RELEASE = 0,
}xpt2046_touch_state;

struct xpt2046_touch_data_struct
{
	unsigned char state;
	int x;
	int y;
};

typedef struct xpt2046_touch_data_struct xpt2046_touch_data_struct_t;
xpt2046_touch_data_struct_t xpt2046_touch_data;

void SDA_IN(void);
void SDA_OUT(void);
void RST_SET(u32 x);
void SCL_SET(u32 x);
void SDA_SET(u32 x);
u32 SDA_READ(u32 x);
void INT_SET(u32 x);

BSP_TOUCH_EXT void bsp_touch__init(void);
BSP_TOUCH_EXT void bsp_touch__scan(void);

#endif/*END OF FILE*/


