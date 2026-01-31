#pragma once

#include "tim.h"			   
#include "main.h"

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);	//DMA回调函数
void WS2812_Update_Task(void);