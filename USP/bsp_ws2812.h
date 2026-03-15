#pragma once
		   
#include "main.h"

//主灯臂流水灯效控制
void ws2812_main_arm_flow_effect(uint8_t r, uint8_t g, uint8_t b, uint8_t active_groups); 
//主灯臂全亮/灭控制
void ws2812_main_arm_full_effect(uint8_t r, uint8_t g, uint8_t b);
//主灯臂阶段亮起矩形块控制
void ws2812_main_arm_stage_effect(uint8_t r, uint8_t g, uint8_t b, uint8_t active_groups);
//副灯臂全亮/灭控制
void ws2812_sub_arm_full_effect(uint8_t r, uint8_t g, uint8_t b);
//副灯臂阶段亮起矩形块控制,输入RGB颜色值和当前组数,根据当前组数阶段性亮起矩形块
void ws2812_sub_arm_stage_effect(uint8_t r, uint8_t g, uint8_t b, uint8_t active_groups);