#pragma once

#include "tim.h"			   
#include "main.h"

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

void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim);	//DMA回调函数
void armshow(ligntarm_name_enum num,light_color_enum color);