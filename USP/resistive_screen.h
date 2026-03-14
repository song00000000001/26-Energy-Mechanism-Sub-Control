#pragma once

#include "main.h"

//检测击打状态转换
typedef enum{
    before_hit=0,
    record_hit,
    after_hit
}HitState_t;

extern uint16_t g_adc_raw[10]; // ADC 原始数据

void Hit_Logic_Task(void);