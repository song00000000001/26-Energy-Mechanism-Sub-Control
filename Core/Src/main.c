/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
//30 + Num * 3 * 8 + 30
#define WS2312_LED_NUM 45
#define PWM_DATA_LEN (WS2312_LED_NUM * 24)
// 定义重置周期数（800KHz 下，1.25us/bit，40个0约 50us）
#define WS2812_RESET_LEN 40 
#define test_num_len (PWM_DATA_LEN + WS2812_RESET_LEN)


#define WS2312_0bit 29
#define WS2312_1bit 50

#define WS2812_delay 15

#define arm_tim1 &htim3
#define arm_channel_1 TIM_CHANNEL_1
#define arm_channel_2 TIM_CHANNEL_3
#define arm_channel_3 TIM_CHANNEL_4
#define arm_tim2 &htim4
#define arm_channel_4 TIM_CHANNEL_1
#define arm_channel_5 TIM_CHANNEL_2
#define arm_channel_6 TIM_CHANNEL_3

uint16_t tim_pwm_dma_buff[5][test_num_len] = {0};//PWM DMA数据缓存
uint8_t Pixel_Buff[WS2312_LED_NUM * 3] = {0};//RGB数据缓存

void Set_Pixel_Color(uint8_t* rgb_buff,uint32_t index,uint8_t R,uint8_t G,uint8_t B)
{
	rgb_buff[(index)*3] = G;
	rgb_buff[(index)*3 + 1] = R;
	rgb_buff[(index)*3 + 2] = B;
}

void Buff_translate(uint8_t* color_buff,uint16_t* dma_row_ptr) //颜色数组转换为码元数组
{   
    uint32_t dat_idx = 0;
	for(uint32_t i = 0;i < (WS2312_LED_NUM*3);i++)
	{
        #if 0
		for(uint8_t k = 0;k < 8;k++)// LSB First: 低位先发
		{
			if ( (color_buff[i] >> k) & 1)
                dma_row_ptr[30 + (i * 8) + k] = WS2312_1bit;
            else 
                color_buff[(i * 8) + k] = WS2312_0bit;
		}
        #else
        for(int8_t k = 7; k >= 0; k--) // MSB First: 高位先发
        {
            if ((color_buff[i] >> k) & 0x01) {
                dma_row_ptr[dat_idx++] = WS2312_1bit; // 50
            } else {
                dma_row_ptr[dat_idx++] = WS2312_0bit; // 29
            }
        }
        #endif
	}
}
/*灯臂
主灯臂有5根灯条,其中1和5并联,2和4并联,3单独一根
分别命名为主臂_外侧,主臂_中侧,主臂_内侧
次灯臂有左右两条；各有2根灯条，而且直接并联；命名为次臂_左，次臂_右。
*/
typedef enum 
{
    main_arm_outside = 0,
    main_arm_middle,
    main_arm_inside,
    sub_arm_left,
    sub_arm_right
}ligntarm_name_enum;

typedef enum 
{
    color_off = 0,
    color_red,
    color_green,
    color_blue
}light_color_enum;

void armshow(uint8_t* rgb_buff,ligntarm_name_enum num,light_color_enum color)
{
    // 1. 根据颜色枚举填充 RGB 缓存
    switch (color)
    {
    case color_red:
        for(uint8_t i=0;i<WS2312_LED_NUM;i++)
        {	
            Set_Pixel_Color(rgb_buff, i, 255, 0, 0);
        }
        break;
    case color_green:
        for(uint8_t i=0;i<WS2312_LED_NUM;i++)
        {	
            Set_Pixel_Color(rgb_buff, i, 0, 255, 0);
        }
        break;
    case color_blue:
        for(uint8_t i=0;i<WS2312_LED_NUM;i++)
        {	
            Set_Pixel_Color(rgb_buff, i, 0, 0, 255);
        }
        break;
    case color_off: 
    default:
        for(uint8_t i=0;i<WS2312_LED_NUM;i++)
        {	
            Set_Pixel_Color(rgb_buff, i, 0, 0, 0);
        }
        break;
    }

    // 2. 获取当前要操作的行地址
    uint16_t* target_row = tim_pwm_dma_buff[num];
    
    // 3. 转换数据
    Buff_translate(rgb_buff, target_row);

    // 4. 启动 DMA 传输
    // 直接传递 target_row 指针并强转，避免了 2D 数组偏移计算错误
    TIM_HandleTypeDef* htim = (num < 3) ? &htim3 : &htim4;
    uint32_t channel;
    // 简单的通道映射映射逻辑
    if(num == main_arm_outside) channel = TIM_CHANNEL_1;
    else if(num == main_arm_middle) channel = TIM_CHANNEL_3;
    else if(num == main_arm_inside) channel = TIM_CHANNEL_4;
    else if(num == sub_arm_left)    channel = TIM_CHANNEL_1; // TIM4
    else                            channel = TIM_CHANNEL_2; // TIM4

    HAL_TIM_PWM_Start_DMA(htim, channel, (uint32_t *)target_row, test_num_len);
	HAL_Delay(WS2812_delay);
}

// DMA 完成回调函数
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    // 判定是哪个定时器触发的
    if (htim->Instance == TIM3) {
        // 传输完成后立即停止 DMA
        // 停止顺序：先停通道，如果有必要可以手动把 CCR 清零
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_3);
        HAL_TIM_PWM_Stop_DMA(htim,TIM_CHANNEL_4);
        // 强制清零 CCR，防止停止瞬间引脚保持高电平
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, 0);
		__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_4, 0);
    }
    else if (htim->Instance == TIM4) {
        // 传输完成后立即停止 DMA
        // 停止顺序：先停通道，如果有必要可以手动把 CCR 清零
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_3);
        // 强制清零 CCR，防止停止瞬间引脚保持高电平
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, 0);
		__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, 0);
    }
}         
                                    
                                    
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1)
    {
        armshow(Pixel_Buff,main_arm_outside,color_red);
        armshow(Pixel_Buff,main_arm_middle,color_green);  
        armshow(Pixel_Buff,main_arm_inside,color_blue);
        armshow(Pixel_Buff,sub_arm_left,color_red);
        armshow(Pixel_Buff,sub_arm_right,color_green);
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
        //HAL_Delay(WS2812_delay);
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enables the Clock Security System
  */
  HAL_RCC_EnableCSS();
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
