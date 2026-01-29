#pragma once

#include "tim.h"			   
#include "main.h"

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
    color_blue
}light_color_enum;

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);	//DMA回调函数
