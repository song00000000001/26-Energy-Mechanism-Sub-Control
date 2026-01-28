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
#define WS2312_LED_NUM 1
#define test_num_len (30 + WS2312_LED_NUM * 3 * 8 + 30)

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

uint16_t tim_pwm_dma_buff[test_num_len] = {0};//PWM DMA数据缓存
uint8_t Pixel_Buff[WS2312_LED_NUM * 3] = {0};//RGB数据缓存

void Set_Pixel_Color(uint8_t* buff,uint32_t index,uint8_t R,uint8_t G,uint8_t B)
{
	buff[(index)*3] = G;
	buff[(index)*3 + 1] = R;
	buff[(index)*3 + 2] = B;
}

void Buff_translate(uint8_t* buff,uint16_t* dma_buff) //颜色数组转换为码元数组
{
	for(uint32_t i = 0;i < (WS2312_LED_NUM*3);i++)
	{
		for(uint8_t k = 0;k < 8;k++)
		{
			if ( (buff[i] >> k) & 1)dma_buff[30 + (i * 8) + k] = WS2312_1bit;
            else dma_buff[30 + (i * 8) + k] = WS2312_0bit;
		}
	}
}
/*灯臂
主灯臂有5根灯条,其中1和5并联,2和4并联,3单独一根
分别命名为主臂_外侧,主臂_中侧,主臂_内侧
次灯臂有左右两条；各有2根灯条，而且直接并联；命名为次臂_左，次臂_右。
*/
typedef enum 
{
    main_arm_outside = 1,
    main_arm_middle = 2,
    main_arm_inside = 3,
    sub_arm_left = 4,
    sub_arm_right = 5
}ligntarm_name_enum;

void armshow_red(uint8_t* buff,uint16_t* dma_buff,uint8_t num)
{
    for(uint8_t i=5;i<45;i++)
    {	
        Set_Pixel_Color(buff, i, 255, 0, 0);
    }
    Buff_translate(buff, dma_buff);
    switch (num)
    {
    case main_arm_outside:
        HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_1, (uint32_t *)dma_buff, test_num_len);
        break;
    case main_arm_middle:
        HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_2, (uint32_t *)dma_buff, test_num_len);
        break;
    case main_arm_inside:
        HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_3, (uint32_t *)dma_buff, test_num_len);
        break;
    case sub_arm_left:
        HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_4, (uint32_t *)dma_buff, test_num_len);
        break;
    case sub_arm_right:
        HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_5, (uint32_t *)dma_buff, test_num_len);
        break;
    default:
        break;
    }
    HAL_Delay(WS2812_delay);
}

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)	//DMA回调函数
{
	/*HAL_TIM_PWM_Stop_DMA(htim,TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop_DMA(htim,TIM_CHANNEL_3);
	HAL_TIM_PWM_Stop_DMA(htim,TIM_CHANNEL_4);*/
}

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
//uint32_t test_buff[10] = {0,0,WS2312_0bit,WS2312_1bit,WS2312_0bit,WS2312_0bit,WS2312_1bit,WS2312_1bit,0,0};
//uint16_t test_buff[2] = {WS2312_0bit,WS2312_1bit};
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


    //测试pwm dma数据
    /*for(uint8_t i=5;i<45;i++)
    {	
        Set_Pixel_Color(Pixel_Buff, i, 255, 0, 0);
    }
    Buff_translate(Pixel_Buff, tim_pwm_dma_buff);
    HAL_Delay(WS2812_delay);*/
	 
    //HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PC6
    //HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_3, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PC8
    //HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PC9
    //HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_1, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PB6
    //HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_2, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PB7
    //HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_3, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PB8

    uint16_t test_buff[10] = {0,0,WS2312_0bit,WS2312_1bit,WS2312_0bit,WS2312_0bit,WS2312_1bit,WS2312_1bit,0,0};
    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (uint32_t*)test_buff, 10);//PC6
	HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_3, (uint32_t*)test_buff, 10);//PC8
    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t*)test_buff, 10);//PC9
    while (1)
    {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
        HAL_Delay(1000);
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
        HAL_Delay(1000);
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
