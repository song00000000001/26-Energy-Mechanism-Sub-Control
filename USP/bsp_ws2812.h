#pragma once

#include "tim.h"			   
#include "main.h"

//灯板led底层控制结构体
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} Indicator_LED_t;
//底层端口宏,用于控制灯板颜色和图案
#define RED_CTRL_PORT    GPIOB
#define RED_CTRL_PIN     GPIO_PIN_0

#define BLUE_CTRL_PORT    GPIOB
#define BLUE_CTRL_PIN     GPIO_PIN_1
//灯板瞄准图案控制底层宏
#define LED_CROSS_CTRL_PORT GPIOA
#define LED_CROSS_CTRL_PIN GPIO_PIN_9
//灯板指示灯颜色总供电控制宏,可以用来快速切换所有环数led的颜色,或者同时关闭两色
#define LED_RED_ENABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_SET);  HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);
#define LED_BLUE_ENABLE HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_SET);  HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);
#define LED_RED_DISABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);  HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);
#define LED_BLUE_DISABLE HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);  HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);
//瞄准图案控制宏
#define LED_SHOW_CROSS_PATTERN HAL_GPIO_WritePin(LED_CROSS_CTRL_PORT, LED_CROSS_CTRL_PIN, GPIO_PIN_SET);
#define LED_SHUT_UP_CROSS_PATTERN HAL_GPIO_WritePin(LED_CROSS_CTRL_PORT, LED_CROSS_CTRL_PIN, GPIO_PIN_RESET);

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);	//DMA回调函数
void WS2812_Update_Task(void);
void LED_Indicator_Task(void) ;