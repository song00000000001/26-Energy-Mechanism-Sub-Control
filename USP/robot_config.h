#pragma once
		   
#include "main.h"
#include "bsp_ws2812.h"
#include "resistive_screen.h"

#define WS2312_LED_NUM 45
#define WS2812_delay 15

extern light_color_enum global_color;


void main_task(void);