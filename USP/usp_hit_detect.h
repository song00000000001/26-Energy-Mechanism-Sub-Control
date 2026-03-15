#pragma once

#include "main.h"
#include "robot_config.h"  
/*todo
song
这里需要引入  WaveCapture_t,目前还没解耦
*/
void Hit_Logic_Task(WaveCapture_t *wave_capture, uint16_t *hit_mask, uint8_t* hit_state);
void Hit_Detection_Init(void);