/**
  ******************************************************************************
  * Copyright (c) 2019 - ~, SCUT-RobotLab Development Team
  * @file    drv_can.c
  * @author  Lv Junyu 13668997406@163.com
  * @brief   Code for CAN driver in STM32 series MCU, supported packaged:
  *          - STM32Cube_FW_F4_V1.24.0.
  *          - STM32Cube_FW_F1_V1.8.0.
  *          - STM32Cube_FW_H7_V1.5.0.
  * @date    2020-07-15
  * @version 1.2
  * @par Change Log：
  * <table>
  * <tr><th>Date        <th>Version  <th>Author    		    <th>Description
  * <tr><td>2019-06-12  <td> 1.0     <td>Mentos Seetoo    <td>Creator
  * <tr><td>2019-11-03  <td> 1.1     <td>Lv Junyu         <td>Remove the precompiled macro \n
  *                                                           Add new function interfaces.
  * <tr><td>2020-07-15  <td> 1.2     <td>Mentos Seetoo    <td>Merge callback_register into init.
  * <tr><td>2023-09-20  <td> 2.0     <td>余俊晖   		  <td>兼容H7系列的FDCAN，精简库代码
  * </table>
  *
  ==============================================================================
							How to use this driver
  ==============================================================================
	@note
	  -# 调用`CAN_Init()`，初始化CAN,设置CAN接收处理函数的指针。
	  -# 如需接收can中的消息，需调用CAN_Filter_Mask_Config配置滤波器（需注意配置格式）
		  示例如下：`CAN_Filter_Mask_Config(&hcan1, CanFilter_1|CanFifo_0|Can_STDID, 0x3ff, 0x3f0)`;
	  -# 在需要用到发送的部分直接调用CANx_SendData()函数

	@warning
	  -# 本模块只能保存一条来自于同一个FIFO的消息(详细见下方HAL库FIFOx中断的
		  实现),请注意及时读走消息。
	  -# 添加预编译宏`USE_FULL_ASSERT`可以启用断言检查。

  ******************************************************************************
  * @attention
  *
  * if you had modified this file, please make sure your code does not have any
  * bugs, update the version Number, write dowm your name and the date. The most
  * important thing is make sure the users will have clear and definite under-
  * standing through your new brief.
  *
  * <h2><center>&copy; Copyright (c) 2019 - ~, SCUT-RobotLab Development Team.
  * All rights reserved.</center></h2>
  ******************************************************************************
*/

#include "srml_config.h"

#if USE_SRML_CAN

/* Includes ------------------------------------------------------------------*/
#include "drv_can.h"
#include "stm32f103xe.h"

/* Private define ------------------------------------------------------------*/
#if defined(CAN3)
	#define CAN_NUM 3
	__CCM const CAN_TypeDef* CAN_Instances[CAN_NUM] = {CAN1, CAN2, CAN3};
#elif defined(CAN2)
	#define CAN_NUM 2
	__CCM const CAN_TypeDef* CAN_Instances[CAN_NUM] = {CAN1, CAN2};
#else
	#define CAN_NUM 1
	__CCM const CAN_TypeDef* CAN_Instances[CAN_NUM] = {CAN1};
#endif
/* Private variables ---------------------------------------------------------*/
__CCM User_CAN_Callback CAN_Callback[CAN_NUM] = {NULL};
__CCM CAN_HandleTypeDef *CAN_Handle[CAN_NUM] = {NULL};
/* Private type --------------------------------------------------------------*/

/* Private function declarations ---------------------------------------------*/
static void Error_Handler(void);
/* function prototypes -------------------------------------------------------*/
/**
 * @brief CAN初始化函数
 *
 * @param hcan CAN句柄
 * @param pFunc 外部回调函数
 * @return uint8_t 返回0表示成功，1表示失败
 */
uint8_t CAN_Init(CAN_HandleTypeDef *hcan, User_CAN_Callback pFunc)
{
	/* Check the parameters */
	assert_param(hcan != NULL);

#if defined(USE_NORMAL_CAN)
	HAL_CAN_Start(hcan);
	__HAL_CAN_ENABLE_IT(hcan, CAN_IT_RX_FIFO0_MSG_PENDING);
	__HAL_CAN_ENABLE_IT(hcan, CAN_IT_RX_FIFO1_MSG_PENDING);
#elif defined(USE_FDCAN)
	//CAN库暂时没有适配数据帧大于8字节的FD帧，为了安全起见，检测FIFO和BUFFER的元素大小不多于8字节
	if(hcan->Init.RxFifo0ElmtSize > FDCAN_DATA_BYTES_8 || hcan->Init.RxFifo1ElmtSize > FDCAN_DATA_BYTES_8 || hcan->Init.RxBufferSize > FDCAN_DATA_BYTES_8)
		Error_Handler();
	// 配置全局过滤器，一定要在HAL_FDCAN_Start前执行，不然没有效果
	// 2-3项为对于被过滤器筛除的数据帧处理方式，如果不配置成FDCAN_REJECT，则被过滤器筛除的数据也会被接收
	// 4-5项为对于遥控帧的处理方式，遥控帧为没有数据段的帧，这里配置为拒收遥控帧
	HAL_FDCAN_ConfigGlobalFilter(hcan, FDCAN_REJECT, FDCAN_REJECT, FDCAN_REJECT_REMOTE, FDCAN_REJECT_REMOTE);
	HAL_FDCAN_Start(hcan);
	// 只用__HAL_FDCAN_ENABLE_IT没有作用，一定要用HAL_FDCAN_ActivateNotification启用中断
	HAL_FDCAN_ActivateNotification(hcan, FDCAN_IT_RX_FIFO0_NEW_MESSAGE, 0);
	HAL_FDCAN_ActivateNotification(hcan, FDCAN_IT_RX_FIFO1_NEW_MESSAGE, 0);
#endif

	for (int i = 0; i < CAN_NUM;i++)
	{
		if (hcan->Instance == CAN_Instances[i])
		{
			CAN_Handle[i] = hcan;
			CAN_Callback[i] = pFunc;
			return SUCCESS;
		}
	}
	Error_Handler();
	return ERROR;
}

/**
 * @brief FDCAN FIFO接收回调函数，FIFO0与FIFO1中断均调用此函数，精简代码
 *
 * @param hcan CAN句柄
 * @param RxLocation FIFO ID的宏，FDCAN_RX_FIFO0 或者 FDCAN_RX_FIFO1
 */
static void CAN_RxFifoCallback(CAN_HandleTypeDef *hcan, uint32_t RxLocation)
{
	/*!< CAN receive buffer */
	static CAN_RxHeaderTypeDef RxHeader;
	static CAN_COB CAN_RxCOB;

	/* Switch to user call back function. */
	if (CAN_GetRxMessage(hcan, RxLocation, &RxHeader, CAN_RxCOB.Data) == HAL_ERROR)
	{
		return;
	}

#if defined(USE_NORMAL_CAN)
	CAN_RxCOB.IdType = RxHeader.IDE >> 2;
	CAN_RxCOB.DLC = RxHeader.DLC;
	if(CAN_RxCOB.IdType == Can_EXTID)
		CAN_RxCOB.ID = RxHeader.ExtId;
	else
		CAN_RxCOB.ID = RxHeader.StdId;

#elif defined(USE_FDCAN)
	if (RxHeader.IdType == FDCAN_STANDARD_ID)
		CAN_RxCOB.IdType = Can_STDID;
	else
		CAN_RxCOB.IdType = Can_EXTID;

	RxHeader.DataLength >>= 16;
	if (RxHeader.DataLength <= 8)
		CAN_RxCOB.DLC = RxHeader.DataLength;
	else
		CAN_RxCOB.DLC = 8 + (RxHeader.DataLength - 8) * 4;

	CAN_RxCOB.ID = RxHeader.Identifier;
#endif

	for (int i = 0; i < CAN_NUM;i++)
	{
		if (hcan->Instance == CAN_Instances[i])
		{
			if(CAN_Callback[i] != NULL)
				CAN_Callback[i](&CAN_RxCOB);
			return;
		}
	}
}


#ifdef USE_NORMAL_CAN
/**
 * @brief 配置CAN的过滤器，模式由用户自行选择
 *
 * @param CAN_Index 	CAN外设序号，例如使用CAN1则传入1
 * @param object_para 	[编号|FIFOx|ID类型]
 * @param Id1 			[ID1]	接收ID
 * @param Id2 			[ID2]	掩码模式下，ID2为掩码值；
 * @param filterType 	过滤器类型，有掩码模式、双ID过滤模式
 */
void CAN_Filter_Config(uint8_t CAN_Index,  uint16_t object_para, uint32_t Id1, uint32_t Id2, CAN_Filter_Type filterType)
{
	CAN_FilterTypeDef CAN_FilterInitStructure;
	/* Check the parameters */
	assert_param(CAN_Index > 0 && CAN_Index <= CAN_NUM);
	assert_param(CAN_Handle[CAN_Index - 1] != NULL);
	if((CAN_Index < 1 || CAN_Index > CAN_NUM) || CAN_Handle[CAN_Index - 1] == NULL)
		Error_Handler();

	/* Communication frame */
	if ((object_para & 0x01) == Can_EXTID)
	{
		CAN_FilterInitStructure.FilterIdHigh = ((Id1 << 3) >> 16) & 0xffff;			/* 接收ID的13-28位 */
		CAN_FilterInitStructure.FilterIdLow = (Id1 << 3 | CAN_ID_EXT) & 0xffff;		/* 接收ID的0-12位，以及设置IDE位扩展帧 */
		CAN_FilterInitStructure.FilterMaskIdHigh = ((Id2 << 3) >> 16) & 0xffff;		/* 掩码模式下为13-28为ID的掩码值；双ID模式下为第二个ID的13-28位 */
		CAN_FilterInitStructure.FilterMaskIdLow = (Id2 << 3 | CAN_ID_EXT) & 0xffff; /* 掩码模式下为0-12为ID、IDE位的掩码值；双ID模式下为第二个ID的0-12位，以及设置IDE位扩展帧 */
	}
	/* Other frame */
	else
	{
		CAN_FilterInitStructure.FilterIdHigh = (Id1 << 5) & 0xffff;		/* 接收ID */
		CAN_FilterInitStructure.FilterIdLow = CAN_ID_STD;				/* 设置IDE位为标准帧 */
		CAN_FilterInitStructure.FilterMaskIdHigh = (Id2 << 5) & 0xffff; /* 掩码模式下为接收ID的掩码值；双ID模式下为第二个ID */
		if (filterType == FILTER_DUAL)
			CAN_FilterInitStructure.FilterMaskIdLow = CAN_ID_STD; /* 双ID模式下，设置IDE位为标准帧 */
		else
			CAN_FilterInitStructure.FilterMaskIdLow = 1 << 2; /* 掩码模式下，设置IDE位掩码为1 */
	}
	CAN_FilterInitStructure.FilterBank = object_para >> 2;					  /* 滤波器序号，0-13给CAN1，14-27给CAN2 */
	CAN_FilterInitStructure.FilterFIFOAssignment = (object_para >> 1) & 0x01; /* 滤波器绑定FIFO */
	CAN_FilterInitStructure.FilterActivation = ENABLE;						  /* 使能滤波器 */
	CAN_FilterInitStructure.FilterMode = filterType;						  /* 滤波器模式，ID掩码模式 or 双ID模式 */
	CAN_FilterInitStructure.FilterScale = CAN_FILTERSCALE_32BIT;			  /* 32位过滤器 */
	CAN_FilterInitStructure.SlaveStartFilterBank = 14;						  /* 将过滤器14-27分配给CAN2*/
	HAL_CAN_ConfigFilter(CAN_Handle[CAN_Index - 1], &CAN_FilterInitStructure);
}

/**
 * @brief CAN发送函数
 *
 * @param CAN_Index 	CAN外设序号，例如使用CAN1则传入1
 * @param CAN_TxMSG 	CAM标准数据帧，包含ID类型、发送ID、数据长度、数据地址
 * @return CAN_SUCCESS: 发送成功
 * @return CAN_LINE_BUSY: 总线繁忙
 */
uint8_t CANx_SendData(uint8_t CAN_Index, CAN_COB *CAN_TxMSG)
{
	static CAN_TxHeaderTypeDef Tx_Header;
	uint32_t used_mailbox;
	/* Check the parameters */
	assert_param(CAN_Index > 0 && CAN_Index <= CAN_NUM);
	assert_param(CAN_Handle[CAN_Index - 1] != NULL);
	if((CAN_Index < 1 || CAN_Index > CAN_NUM) || CAN_Handle[CAN_Index - 1] == NULL || CAN_TxMSG->DLC > 8)
		Error_Handler();

	Tx_Header.StdId = CAN_TxMSG->ID;
	Tx_Header.ExtId = CAN_TxMSG->ID;
	Tx_Header.IDE = CAN_TxMSG->IdType * CAN_ID_EXT;
	Tx_Header.RTR = 0;
	Tx_Header.DLC = CAN_TxMSG->DLC;

	if (HAL_CAN_AddTxMessage(CAN_Handle[CAN_Index - 1], &Tx_Header, CAN_TxMSG->Data, &used_mailbox) != HAL_OK)
	{
		return CAN_LINE_BUSY;
	}

	return CAN_SUCCESS;
}

/*HAL库FIFO0中断*/
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	CAN_RxFifoCallback(hcan, CAN_FILTER_FIFO0);
}
/*HAL库FIFO1中断*/
void HAL_CAN_RxFifo1MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
	CAN_RxFifoCallback(hcan, CAN_FILTER_FIFO1);
}
#elif defined(USE_FDCAN)
/**
 * @brief 配置CAN的过滤器，模式由用户自行选择
 *
 * @param CAN_Index 	CAN外设序号，例如使用CAN1则传入1
 * @param object_para 	[编号|FIFOx|ID类型]
 * @param Id1 			[ID1]	接收ID
 * @param Id2 			[ID2]	掩码模式下，ID2为掩码值；
 * @param filterType 	过滤器类型，有掩码模式、双ID过滤模式、[ID1, ID2]区间过滤模式
 */
void CAN_Filter_Config(uint8_t CAN_Index, uint16_t object_para, uint32_t Id1, uint32_t Id2, CAN_Filter_Type filterType)
{
	FDCAN_FilterTypeDef CAN_FilterInitStructure;
	/* Check the parameters */
	assert_param(CAN_Index > 0 && CAN_Index <= CAN_NUM);
	assert_param(CAN_Handle[CAN_Index - 1] != NULL);
	if((CAN_Index < 1 || CAN_Index > CAN_NUM) || CAN_Handle[CAN_Index - 1] == NULL)
		Error_Handler();

	/* Communication frame */
	CAN_FilterInitStructure.IdType = (object_para & 0x1) * FDCAN_EXTENDED_ID; // 接收ID类型，标准帧or扩展帧
	CAN_FilterInitStructure.FilterType = filterType;						  // 过滤器类型
	CAN_FilterInitStructure.FilterIndex = (object_para >> 2);				  // 过滤器编号，标准帧和扩展帧的筛选器id可以相同
	CAN_FilterInitStructure.FilterConfig = ((object_para >> 1) & 0x1) + 1;	  // 筛选器设置，FIFO0 or FIFO1
	CAN_FilterInitStructure.FilterID1 = Id1;								  // ID1
	CAN_FilterInitStructure.FilterID2 = Id2;								  // ID2
	CAN_FilterInitStructure.RxBufferIndex = NULL;
	CAN_FilterInitStructure.IsCalibrationMsg = NULL;

	HAL_FDCAN_ConfigFilter(CAN_Handle[CAN_Index - 1], &CAN_FilterInitStructure);
}

/**
 * @brief CAN发送函数，用于发送标准帧，不能发送FD帧
 *
 * @param CAN_Index 	CAN外设序号，例如使用CAN1则传入1
 * @param CAN_TxMSG 	CAM消息包，包含ID类型、发送ID、数据长度、数据地址
 * 						其中数据长度不应超过8
 * @return CAN_SUCCESS: 发送成功
 * @return CAN_LINE_BUSY: 总线繁忙
 */
uint8_t CANx_SendData(uint8_t CAN_Index, CAN_COB *CAN_TxMSG)
{
	/* Check the parameters */
	assert_param(CAN_Index > 0 && CAN_Index <= CAN_NUM);
	assert_param(CAN_Handle[CAN_Index - 1] != NULL);
	assert_param(CAN_TxMSG->DLC < 9);
	if((CAN_Index < 1 || CAN_Index > CAN_NUM) || CAN_Handle[CAN_Index - 1] == NULL || CAN_TxMSG->DLC > 8)
		Error_Handler();

	static FDCAN_TxHeaderTypeDef TxHeader;
	TxHeader.Identifier = CAN_TxMSG->ID;
	TxHeader.IdType = (CAN_TxMSG->IdType) * FDCAN_EXTENDED_ID;
	TxHeader.TxFrameType = FDCAN_DATA_FRAME;
	TxHeader.DataLength = (CAN_TxMSG->DLC) << 16;
	TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
	TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
	TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
	TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
	TxHeader.MessageMarker = 0;

	if (HAL_FDCAN_AddMessageToTxFifoQ(CAN_Handle[CAN_Index - 1], &TxHeader, CAN_TxMSG->Data) != HAL_OK)
	{
		return CAN_LINE_BUSY;
	}

	return CAN_SUCCESS;
}

/*HAL库FIFO0中断*/
void HAL_FDCAN_RxFifo0Callback(CAN_HandleTypeDef *hcan, uint32_t RxFifo0ITs)
{
	CAN_RxFifoCallback(hcan, FDCAN_RX_FIFO0);
}
/*HAL库FIFO1中断*/
void HAL_FDCAN_RxFifo1Callback(CAN_HandleTypeDef *hcan, uint32_t RxFifo0ITs)
{
	CAN_RxFifoCallback(hcan, FDCAN_RX_FIFO1);
}
#endif /* USE_FDCAN */

/**
 * @brief  This function is executed in case of error occurrence.
 * @param  None
 * @retval None
 */
static void Error_Handler(void)
{
  /* Normally the program would never run here. */
  while(1){}
}

#endif /* USE_SRML_CAN */

/************************ COPYRIGHT(C) SCUT-ROBOTLAB **************************/
