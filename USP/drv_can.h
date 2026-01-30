/**
 ******************************************************************************
 * Copyright (c) 2019 - ~, SCUT-RobotLab Development Team
 * @file    drv_can.h
 * @author  Lv Junyu 13668997406@163.com
 * @brief   Code for CAN driver in STM32 series MCU, supported packaged:
 *          - STM32Cube_FW_F4_V1.24.0.
 *          - STM32Cube_FW_F1_V1.8.0.
 *          - STM32Cube_FW_H7_V1.5.0.
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
#ifndef _DRV_CAN_H
#define _DRV_CAN_H

/* Includes ------------------------------------------------------------------*/
#include "srml_std_lib.h"

#if defined(USE_NORMAL_CAN)
#define CAN_GetRxMessage HAL_CAN_GetRxMessage
#elif defined(USE_FDCAN)
#define CAN_HandleTypeDef FDCAN_HandleTypeDef
#define CAN_RxHeaderTypeDef FDCAN_RxHeaderTypeDef
#define CAN_TypeDef FDCAN_GlobalTypeDef

#define CAN_GetRxMessage HAL_FDCAN_GetRxMessage

#define CAN1 FDCAN1
#define CAN2 FDCAN2
#if defined(FDCAN3)
#define CAN3 FDCAN3
#endif
#endif

#ifdef __cplusplus
extern "C"
{
#endif
/* Private macros ------------------------------------------------------------*/
/* Private type --------------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
#define CAN_LINE_BUSY 0
#define CAN_SUCCESS 1
#define CAN_FIFO_SIZE 1024

#define CanFilter_0 (0 << 2)
#define CanFilter_1 (1 << 2)
#define CanFilter_2 (2 << 2)
#define CanFilter_3 (3 << 2)
#define CanFilter_4 (4 << 2)
#define CanFilter_5 (5 << 2)
#define CanFilter_6 (6 << 2)
#define CanFilter_7 (7 << 2)
#define CanFilter_8 (8 << 2)
#define CanFilter_9 (9 << 2)
#define CanFilter_10 (10 << 2)
#define CanFilter_11 (11 << 2)
#define CanFilter_12 (12 << 2)
#define CanFilter_13 (13 << 2)
#define CanFilter_14 (14 << 2)
#define CanFilter_15 (15 << 2)
#define CanFilter_16 (16 << 2)
#define CanFilter_17 (17 << 2)
#define CanFilter_18 (18 << 2)
#define CanFilter_19 (19 << 2)
#define CanFilter_20 (20 << 2)
#define CanFilter_21 (21 << 2)
#define CanFilter_22 (22 << 2)
#define CanFilter_23 (23 << 2)
#define CanFilter_24 (24 << 2)
#define CanFilter_25 (25 << 2)
#define CanFilter_26 (26 << 2)
#define CanFilter_27 (27 << 2)

#define CanFifo_0 (0 << 1)
#define CanFifo_1 (1 << 1)

	/* Exported types ------------------------------------------------------------*/
	typedef enum
	{
		Can_STDID = 0, // 标准帧
		Can_EXTID = 1  // 扩展帧
	} CAN_ID_Type;

	/**
	 * @brief CAN message data type(Communication Object/标准数据帧)
	 */
	typedef struct
	{
		CAN_ID_Type IdType; // 发送or接收的ID类型
		uint32_t ID;		// 发送or接收的ID
		uint8_t DLC;		// 发送or接收的数据长度
		uint8_t Data[8];	// 发送or接收的数据
	} CAN_COB;

	typedef void (*User_CAN_Callback)(CAN_COB *); // 用户回调函数类型

#if defined(USE_FDCAN)
	typedef enum
	{
		FILTER_RANGE = FDCAN_FILTER_RANGE,				  /*!< [ID1, ID2]区间过滤模式 */
		FILTER_DUAL = FDCAN_FILTER_DUAL,				  /*!< 双ID过滤模式 */
		FILTER_MASK = FDCAN_FILTER_MASK,				  /*!< 掩码模式 */
		FILTER_RANGE_NO_EIDM = FDCAN_FILTER_RANGE_NO_EIDM /*!< [ID1, ID2]区间过滤模式，未使用XIDAM掩码 */
	} CAN_Filter_Type;
#else
typedef enum
{
	FILTER_MASK = CAN_FILTERMODE_IDMASK, /*!< 掩码模式 */
	FILTER_DUAL = CAN_FILTERMODE_IDLIST	 /*!< 双ID过滤模式 */
} CAN_Filter_Type;
#endif
	/* Exported variables ---------------------------------------------------------*/
	/* Exported function declarations ---------------------------------------------*/
	/**
	 * @brief CAN初始化函数
	 *
	 * @param hcan CAN句柄
	 * @param pFunc 外部回调函数
	 * @return uint8_t 返回0表示成功，1表示失败
	 */
	uint8_t CAN_Init(CAN_HandleTypeDef *hcan, User_CAN_Callback pFunc);

	/**
	 * @brief CAN发送函数
	 *
	 * @param CAN_Index 	CAN外设序号，例如使用CAN1则传入1
	 * @param CAN_TxMSG 	CAM标准数据帧，包含ID类型、发送ID、数据长度、数据地址
	 * @return CAN_SUCCESS: 发送成功
	 * @return CAN_LINE_BUSY: 总线繁忙
	 */
	uint8_t CANx_SendData(uint8_t CAN_Index, CAN_COB *CAN_TxMSG);

	/**
	 * @brief 配置CAN的过滤器，模式由用户自行选择
	 *
	 * @param CAN_Index 	CAN外设序号，例如使用CAN1则传入1
	 * @param object_para 	[编号|FIFOx|ID类型]
	 * @param Id1 			[ID1]	接收ID
	 * @param Id2 			[ID2]	掩码模式下，ID2为掩码值；
	 * @param filterType 	过滤器类型，有掩码模式、双ID过滤模式
	 */
	void CAN_Filter_Config(uint8_t CAN_Index, uint16_t object_para, uint32_t Id1, uint32_t Id2, CAN_Filter_Type filterType);

    void User_CAN1_RxCpltCallback(CAN_COB *CAN_RxCOB);
#define CAN_Filter_Mask_Config(CAN_Index, object_para, Id, MaskId) CAN_Filter_Config(CAN_Index, object_para, Id, MaskId, FILTER_MASK)

#ifdef __cplusplus
}
#endif

#endif
/************************ COPYRIGHT(C) SCUT-ROBOTLAB **************************/
