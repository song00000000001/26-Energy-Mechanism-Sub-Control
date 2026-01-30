/**
  ******************************************************************************
  * Copyright (c) 2019 - ~, SCUT-RobotLab Development Team
  * @file    srml_std_lib.h
  * @author  EnnisKoh 8762322@qq.com
  * @brief   收容各种难以收容但又需要统一标准的东西
  * @date    2021-04-29
  * @version 1.1
  *
  ******************************************************************************
  * @attention
  *
  * if you had modified this file, please make sure your code does not have many
  * bugs, update the version Number, write dowm your name and the date, the most
  * important is make sure the users will have clear and definite understanding
  * through your new brief.
  *
  * <h2><center>&copy; Copyright (c) 2019 - ~, SCUT-RobotLab Development Team.
  * All rights reserved.</center></h2>
  ******************************************************************************
  */

#ifndef _SRML_STD_LIB_H_
#define _SRML_STD_LIB_H_

/* Includes ------------------------------------------------------------------*/
#include <stdint.h>
#include <stdlib.h>
#include <string.h> //memcpy的头文件
#include <math.h>

#if __ARMCLIB_VERSION >= 6000000
	#define USE_AC6
#elif __ARMCLIB_VERSION >= 5000000
	#define USE_AC5
#endif

#if defined(USE_HAL_DRIVER)

	#if defined(STM32F405xx) || defined(STM32F407xx)
		#define USE_STM32F4
	#elif defined(STM32F334x8)
		#define USE_STM32F3
	#elif defined(STM32F103xx) || defined(STM32F103xE)
		#define USE_STM32F1
	#elif defined(STM32H750xx) || defined(STM32H723xx)
		#define USE_STM32H7
	#endif	

	#if defined(USE_STM32F4)
		#include <stm32f4xx_hal.h>
	#elif defined(USE_STM32F3)
		#include <stm32f3xx_hal.h>
	#elif defined(USE_STM32F1)
		#include <stm32f1xx_hal.h>
	#elif defined(USE_STM32H7)
		#include <stm32h7xx_hal.h>
	#endif
	
#endif

#if defined(USE_STM32F1) || defined(USE_STM32F4) || defined(USE_STM32F3)
	#define USE_NORMAL_CAN
  
  #define __CCM           __attribute__((section(".CCM")))  //放在变量声明的类型前，变量将会强制放在CCM
	#define __SRAM          __attribute__((section(".SRAM"))) //放在变量声明的类型前，变量将会强制放在SRAM
  #define __DTCM          __CCM //与H7兼容
	#define SRAM_BASE_ADDR SRAM_BASE
#elif defined(USE_STM32H7)
	#define USE_FDCAN
	#define USE_CACHE

	#define __DTCM          __attribute__((section(".DTCM"))) //放在变量声明的类型前，变量将会强制放在DTCM
  #define __SRAM          __attribute__((section(".SRAM")))
  #define __CCM           __DTCM //与F4兼容
	#define SRAM_BASE_ADDR  D1_AXISRAM_BASE
#endif

#define __IS_ADDR_AT_SRAM(ptr) (uint32_t)ptr >= SRAM_BASE_ADDR
#define __IS_DATA_AT_SRAM(data) (uint32_t)&data >= SRAM_BASE_ADDR

#ifndef PI
  #define PI               3.14159265358979f
#endif

#define LIMIT_MIN_MAX(x,min,max) (x) = (((x)<=(min))?(min):(((x)>=(max))?(max):(x)))

#define RAD(deg) ((deg) / 180 * PI)
#define DEG(rad) ((rad) / PI * 180)

static const float DEGREE_TO_RAD = 2 * PI / 360.f;
static const float RAD_TO_DEGREE = 360 / (2 * PI);
/* Private macros ------------------------------------------------------------*/
/* Private type --------------------------------------------------------------*/
/* Exported macros -----------------------------------------------------------*/
/* Exported types ------------------------------------------------------------*/
#ifdef  __cplusplus
using std::free;
using std::malloc;

/**
 * @brief 变量变化检测宏，类内使用要注意
 * 
 */
#define __IS_CHG(var) ({ \
   volatile static decltype(var) last = {}; \
   bool _judge_result_ = (var != last ? true : false); \
   last = var; \
   _judge_result_; \
})

 

namespace std_lib
{
  /* Exported variables --------------------------------------------------------*/

  /* Exported function declarations --------------------------------------------*/
  template<class T>
  inline T constrain(T input,T min,T max)
  {
    if (input <= min)
      return min;
    else if(input >= max)
      return max;
    return input;
  }

  uint16_t get_gpio_pin_num(uint16_t GPIO_Pin);
  uint32_t float_to_uint(float x, float x_min, float x_max, uint8_t bits);
  float uint_to_float(int x_int, float x_min, float x_max, uint8_t bits);

  uint8_t CRC8(const void *_data, uint16_t length, uint8_t polynomial = 0x31);
  uint16_t CRC16(const void *_data, uint16_t length, uint16_t polynomial = 0x8005);
  uint32_t CRC32(const void *_data, uint16_t length, uint32_t polynomial = 0x04C11DB7);

  float DeadZone_Process(float num, float DZ_min, float DZ_max, float DZ_num = 0);
}

#endif /* __cplusplus */

#endif /* _SHIT_MOUNTAIN_H_ */

/************************ COPYRIGHT(C) SCUT-ROBOTLAB **************************/
