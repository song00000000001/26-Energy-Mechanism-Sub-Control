#pragma once

#include "main.h"
#include "robot_config.h"  

typedef struct {
    uint8_t pending;
    uint8_t hit_index;
    uint8_t adc_pin_map_index;
} HitEvent_t;

void Hit_Detection_Init(void);
void Hit_Logic_Task(void);

/* 检测层只负责生成事件 */
void Hit_Detection(HitEvent_t *event);

/* 给通信层调试发波形时读取波形数据 */
//增加一个只读波形访问接口，后面通信发 VOFA 不需要再持有全局 wave_capture
const WaveCapture_t* Hit_GetWaveCapture(void);
uint8_t Hit_Get_adc_pin_map_index(uint8_t hit_index);