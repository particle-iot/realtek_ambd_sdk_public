
#define _BSP_TOUCH_C_

#include "bsp_touch.h"


void SDA_IN(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = SDA_PIN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	GPIO_Init(&GPIO_InitStructure);


}

void SDA_OUT(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Pin = SDA_PIN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	PAD_PullCtrl(SDA_PIN, GPIO_PuPd_UP);
	GPIO_Init(&GPIO_InitStructure);
	PAD_PullCtrl(SDA_PIN, GPIO_PuPd_UP);
}

void RST_SET(u32 x)
{
	GPIO_WriteBit(RST_PIN, x);
}

void SCL_SET(u32 x)
{
	GPIO_WriteBit(SCL_PIN, x);
}

void SDA_SET(u32 x)
{
	GPIO_WriteBit(SDA_PIN, x);
}

void INT_SET(u32 x)
{}


/****************************************************
* �������� ��
* ��    �� ����Ƭ��������ʼ�ź�
* ��ڲ��� ����
* ���ز��� ����
* ע������ ��
*****************************************************/
void FT6236_Start(void)					
{
	SDA_OUT();     		//sda�����
	DelayUs(3);
	FT6236_SDA(1);									
	FT6236_SCL(1);		//SCL��С�ߵ�ƽ����:0.6us
	DelayUs(4);		//��ʼ�źŵ���С����ʱ��:0.6us
	FT6236_SDA(0);		//SCL�ߵ�ƽ�ڼ䣬SDA��һ���½��ر�ʾ��ʼ�ź�
	DelayUs(4);		//��ʼ�źŵ���С����ʱ��:0.6us
	FT6236_SCL(0);		//��ס����,Ϊ����������ַ��׼��;
	DelayUs(2);		//SCL��С�͵�ƽ����:1.2us,��RETʵ��
}

/****************************************************
* �������� ��
* ��    �� ����Ƭ������ֹͣ�ź�
* ��ڲ��� ����
* ���ز��� ����
* ע������ ��
*****************************************************/
void FT6236_Stop(void)							
{
	SDA_OUT();     		//sda�����	
	DelayUs(3);	
	FT6236_SCL(1);		//SCL��С�ߵ�ƽ����:0.6us		
	DelayUs(4);		//ֹͣ�źŵ���С����ʱ��:0.6us
	FT6236_SDA(0);	
	DelayUs(4);
	FT6236_SDA(1);		//SCL�ߵ�ƽ�ڼ䣬SDA��һ�������ر�ʾֹͣ�ź�
	DelayUs(2);						
}

/****************************************************
* �������� ��
* ��    �� ����Ƭ������Ӧ���ź�
* ��ڲ��� ����
* ���ز��� ����
* ע������ ����Ƭ����1B���ݺ���һ��Ӧ���ź�
*****************************************************/
void FT6236_McuACK(void)							
{
	FT6236_SCL(0);	
	SDA_OUT();     		//sda�����	
	DelayUs(3);
	FT6236_SDA(0);	
	DelayUs(2);																	
	FT6236_SCL(1);		//SCL��С�ߵ�ƽ����:0.6us
	DelayUs(2);
	FT6236_SCL(0);		//SCL��С�͵�ƽ����:1.2us
}

/****************************************************
* �������� ��
* ��    �� ����Ƭ�����ͷ�Ӧ���ź�
* ��ڲ��� ����
* ���ز��� ����
* ע������ ����Ƭ��������ֹͣǰ����һ����Ӧ���ź�
*****************************************************/
void FT6236_McuNACK(void)
{
	FT6236_SCL(0);	
	SDA_OUT();     				//sda�����	
	DelayUs(3);
	FT6236_SDA(1);	
	DelayUs(2);																	
	FT6236_SCL(1);				//SCL��С�ߵ�ƽ����:0.6us
	DelayUs(2);
	FT6236_SCL(0);				//SCL��С�͵�ƽ����:1.2us
}

/****************************************************
* �������� ��
* ��    �� ����Ƭ�����FT6236������Ӧ���ź�
* ��ڲ��� ����
* ���ز��� ��1������Ӧ��ʧ��
			 0������Ӧ��ɹ�
* ע������ ����Ƭ��д1����ַ/���ݺ���
			 ȫ�ֱ���RevAckF:�յ�FT6236Ӧ���źŵı�־λ,Ϊ0��ʾ�յ�
*****************************************************/
u8 FT6236_CheckAck(void)							
{
	u8 ucErrTime=0;
	SDA_IN();      				//SDA����Ϊ����
	FT6236_SDA(1);
	FT6236_SCL(1);				//ʹSDA��������Ч;SCL��С�ߵ�ƽ����:0.6us
	DelayUs(3);
	while(FT6236_SDA_Read)
	{	
		ucErrTime++;
		if(ucErrTime>250)		//��Ӧ��
		{
			FT6236_Stop();	
			return 1;
		}
		DelayUs(2);
	}
	FT6236_SCL(0);
	return 0;
}

/****************************************************
* �������� ��
* ��    �� ����Ƭ����IIC���߷���1B�ĵ�ַ/����
* ��ڲ��� �������͵�1B��ַ/����
* ���ز��� ����
* ע������ ������һ�����������ݷ��͹���;������˳���ǴӸߵ���
*****************************************************/
void FT6236_WrOneByte(u8 dat)						
{
	u8 i;						
	SDA_OUT();     				//sda�����	
	FT6236_SCL(0);				//����ʱ�ӿ�ʼ���ݴ���
	DelayUs(3);
	for(i = 8; i > 0; i--)		//8λ1B��ַ/���ݵĳ���
	{
		if(dat & 0x80) 		
			FT6236_SDA(1);		//����"1"		
		else
			FT6236_SDA(0);		//����"0"
		FT6236_SCL(1);			//ʹSDA�ϵ�������Ч
		DelayUs(2);			//SCL��С�ߵ�ƽ����:0.6us							
		FT6236_SCL(0);			//SCL��С�͵�ƽ����:1.2us
		DelayUs(2);
		dat <<= 1;				//������������1λ,Ϊ��λ����׼��	
	}		
}

/****************************************************
* �������� ��
* ��    �� ����Ƭ����IIC���߽���1B������
* ��ڲ��� ����
* ���ز��� ���յ���1B����
* ע������ ������һ�����������ݽ��չ���;�Ӹߵ��͵�˳���������
*****************************************************/
u8 FT6236_RdOneByte(void)						
{
	u8 i,dat = 0;				//��������λ���������ݴ浥Ԫ
	SDA_IN();						//SDA����Ϊ����
	DelayUs(2);	
	FT6236_SDA(1);			//ʹ������,׼����ȡ����
	DelayUs(2);
	for(i = 8;i > 0;i--)
	{
		FT6236_SCL(0);
		DelayUs(2);
		FT6236_SCL(1);
		dat <<= 1;
		if(FT6236_SDA_Read)
			dat++;
		DelayUs(2);			//SCL��С�͵�ƽ����:1.2us
	}
	FT6236_SDA(1);		
	return(dat);				//����1B������
}

//��FT6236д��һ������
//reg:��ʼ�Ĵ�����ַ
//buf:���ݻ�������
//len:д���ݳ���
//����ֵ:0,�ɹ�;1,ʧ��.
u8 FT6236_WR_Reg(u16 reg,u8 *buf,u8 len)
{
	u8 i;
	u8 ret=0;
	FT6236_Start();	 
	FT6236_WrOneByte(FT_CMD_WR);	//����д���� 	 
	FT6236_CheckAck(); 	 										  		   
	FT6236_WrOneByte(reg&0XFF);   	//���͵�8λ��ַ
	FT6236_CheckAck();  
	for(i=0;i<len;i++)
	{	   
    	FT6236_WrOneByte(buf[i]);  	//������
		ret=FT6236_CheckAck();
		if(ret)break;  
	}
    FT6236_Stop();					//����һ��ֹͣ����	    
	return ret; 
}
//��FT6236����һ������
//reg:��ʼ�Ĵ�����ַ
//buf:���ݻ�������
//len:�����ݳ���			  
void FT6236_RD_Reg(u16 reg,u8 *buf,u8 len)
{
	u8 i; 
 	FT6236_Start();	
 	FT6236_WrOneByte(FT_CMD_WR);   	//����д���� 	 
	FT6236_CheckAck(); 	 										  		   
 	FT6236_WrOneByte(reg&0XFF);   	//���͵�8λ��ַ
	FT6236_CheckAck();  
 	FT6236_Start();  	 	   
	FT6236_WrOneByte(FT_CMD_RD);   	//���Ͷ�����		   
	FT6236_CheckAck();	  
	for(i=0;i<len;i++)
	{	   
		*buf++ = FT6236_RdOneByte();		//����1B���ݵ��������ݻ�������
		FT6236_McuACK();					//����Ӧ��λ	  
	} 
	FT6236_McuNACK();						//n���ֽڶ���,���ͷ�Ӧ��λ
    FT6236_Stop();					//����һ��ֹͣ����	  
} 


/* 
**��������FT6236_Init
**�����������
**����ֵ����
**���ܣ���ʼ��FT6236����
*/  
void bsp_touch__init(void)
{
	u8 temp; 
	GPIO_InitTypeDef GPIO_InitStructure;					//����һ��GPIO_InitTypeDef���͵Ľṹ��

	GPIO_InitStructure.GPIO_Pin = SCL_PIN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	PAD_PullCtrl(SCL_PIN, GPIO_PuPd_UP);
	GPIO_Init(&GPIO_InitStructure);
	PAD_PullCtrl(SCL_PIN, GPIO_PuPd_UP);
	GPIO_WriteBit(SCL_PIN, 1);

	GPIO_InitStructure.GPIO_Pin = SDA_PIN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	PAD_PullCtrl(SDA_PIN, GPIO_PuPd_UP);
	GPIO_Init(&GPIO_InitStructure);
	PAD_PullCtrl(SDA_PIN, GPIO_PuPd_UP);
	GPIO_WriteBit(SDA_PIN, 1);

	GPIO_InitStructure.GPIO_Pin = RST_PIN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	PAD_PullCtrl(RST_PIN, GPIO_PuPd_UP);
	GPIO_Init(&GPIO_InitStructure);
	PAD_PullCtrl(RST_PIN, GPIO_PuPd_UP);
	GPIO_WriteBit(RST_PIN, 1);

	GPIO_InitStructure.GPIO_Pin = INT_PIN;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;
	PAD_PullCtrl(INT_PIN, GPIO_PuPd_UP);
	GPIO_Init(&GPIO_InitStructure);
	PAD_PullCtrl(INT_PIN, GPIO_PuPd_UP);
	GPIO_WriteBit(INT_PIN, 1);
	
	
	FT6236_RST(0);
	DelayMs(50);
	FT6236_RST (1);
	DelayMs(100);
	FT6236_SDA(1);
	FT6236_SCL(1);
	DelayMs(10);
	temp=0;
	FT6236_WR_Reg(FT_DEVIDE_MODE,&temp,1);	//������������ģʽ 
 	temp=12;								//������Чֵ��22��ԽСԽ����	
 	FT6236_WR_Reg(FT_ID_G_THGROUP,&temp,1);	//���ô�����Чֵ
 	temp=12;								//�������ڣ�����С��12�����14
 	FT6236_WR_Reg(FT_ID_G_PERIODACTIVE,&temp,1); 
/******************************************************/
}
const u16 FT6236_TPX_TBL[5]=
{
	FT_TP1_REG,
	FT_TP2_REG,
	FT_TP3_REG,
	FT_TP4_REG,
	FT_TP5_REG
};

TouchPointRefTypeDef TPR_Structure; 

void FT6236_Scan(void)
{
	u8 i=0;
	u8 sta = 0;
	u8 buf[4] = {0};    
	FT6236_RD_Reg(0x02,&sta,1);//��ȡ�������״̬  	   
 	if(sta & 0x0f)	//�ж��Ƿ��д����㰴�£�0x02�Ĵ����ĵ�4λ��ʾ��Ч�������
 	{
 		TPR_Structure.TouchSta = ~(0xFF << (sta & 0x0F));	//~(0xFF << (sta & 0x0F))����ĸ���ת��Ϊ�����㰴����Ч��־
// 		for(i=0;i<5;i++)	                                //�ֱ��жϴ�����1-5�Ƿ񱻰���
// 		{
 			if(TPR_Structure.TouchSta & (1<<i))			    //��ȡ����������
 			{											    //���������ȡ��Ӧ��������������
 				FT6236_RD_Reg(FT6236_TPX_TBL[i],buf,4);	//��ȡXY����ֵ
				TPR_Structure.x[i]=((u16)(buf[0]&0X0F)<<8)+buf[1];
				TPR_Structure.y[i]=((u16)(buf[2]&0X0F)<<8)+buf[3];
 				if((buf[0]&0XC0)!=0X80)
 				{
					TPR_Structure.x[i]=TPR_Structure.y[i]=0;//������contact�¼�������Ϊ��Ч	
					return;
				}
 			}
// 		}
 		TPR_Structure.TouchSta |= TP_PRES_DOWN;     //�������±��
 	}
 	else
 	{
 		if(TPR_Structure.TouchSta &TP_PRES_DOWN) 	//֮ǰ�Ǳ����µ�
 			TPR_Structure.TouchSta &= ~0x80;        //�����ɿ����	
 		else
 		{
 			TPR_Structure.x[0] = 0;
 			TPR_Structure.y[0] = 0;
 			TPR_Structure.TouchSta &=0xe0;	//�����������Ч���
 		}
 	}
}




void bsp_touch__scan(void)
{
	u16 x_val,y_val;
	
	FT6236_Scan();
	
	if(TPR_Structure.TouchSta & TP_PRES_DOWN)
	{
		
		xpt2046_touch_data.state = TOUCH_PRESS;
		
		xpt2046_touch_data.x = TPR_Structure.x[0];

		xpt2046_touch_data.y = TPR_Structure.y[0];		
	}
	else
	{
		xpt2046_touch_data.state = TOUCH_RELEASE;
	}
	

}

