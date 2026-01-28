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
#define WS2312_0bit 29
#define WS2312_1bit 50

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
}
//测试发现,dma搬数据的长度和输入的值无关,总是会搬16个数据,多余的数据是固定的不知道啥值,所以给16个测试一下
/*
static uint16_t test_buff[3][16] = {{WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,
                                    WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit,WS2312_0bit},
                                    {WS2312_1bit,WS2312_0bit,WS2312_1bit,WS2312_0bit,WS2312_1bit,WS2312_0bit,WS2312_1bit,WS2312_0bit,
                                     WS2312_0bit,WS2312_1bit,WS2312_0bit,WS2312_1bit,WS2312_0bit,WS2312_1bit,WS2312_0bit,WS2312_1bit},
                                    {WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,
                                     WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit,WS2312_1bit}};
*/
static uint16_t test_buff[3][27] = {{5,10,15,20,25 ,35,40,45,50,55 ,60,65,70,75,80 ,80,80,50,50,50 ,30,30,30,20,0 ,0,0}, 
									{80,75,70,65,60 ,55,50,45,40,35 ,30,25,20,15,10 ,80,50,80,50,80 ,40,60,10,20,0 ,0,0},            
                                    {60,29,29,29,29 ,29,29,29,29,29 ,29,44,29,10,21 ,80,50,60,40,30 ,50,60,40,20,0 ,0,0}};           
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

    //HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (uint32_t*)test_buff, 10);//PC6
	//HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_3, (uint32_t*)test_buff, 10);//PC8
    //HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t*)test_buff, 10);//PC9
    //HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_1, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PB6
    //HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_2, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PB7
    //HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_3, (uint32_t*)tim_pwm_dma_buff, test_num_len);//PB8
    while (1)
    {
		HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (uint32_t*)(test_buff[0]),27);//PC6
		HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_3, (uint32_t*)(test_buff[1]), 27);//PC8
        HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t*)(test_buff[2]), 27);//PC9
        HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
        HAL_Delay(200);
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
