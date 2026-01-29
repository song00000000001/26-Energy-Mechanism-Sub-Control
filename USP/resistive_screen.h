#pragma once

#include "main.h"
#include "robot_config.h"

extern uint16_t g_adc_raw[10]; // ADC 原始数据

void ADC_System_Start(void);
void Hit_Logic_Task(void);
void ADC_Sampling_Task(void);