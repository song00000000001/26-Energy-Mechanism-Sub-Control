#include "resistive_screen.h"
#include "robot_config.h"

ADCBuffers_t adc_buffers={
    .HIT_THRESHOLD = 200,  // ADC 击打判定阈值 (根据实际压力调整)
    .HIT_CONFIRM_COUNT = 2,        // 连续N次采样超过阈值则认为击打
    .adc_pin_map = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9} // 映射表，根据实际连线调整
};

//由于连线不同,交换adc引脚映射表:
//实际从1到10的对应的顺序为:

// 击打判定逻辑 (100Hz 运行，即 10ms 检查一次)
void Hit_Logic_Task(void) {
    for(int i = 0; i < 10; i++) {
        if (adc_buffers.adc_raw[adc_buffers.adc_pin_map[i]] > adc_buffers.HIT_THRESHOLD)  
            //if (adc_buffers.hit_counters[i] < 255) 
            adc_buffers.hit_counters[i]++;// 超过阈值，计数器增加
		#if 0
        else {
           
            // 4. 低于阈值则计数器回落
            if (adc_buffers.hit_counters[i] > 0) {
                adc_buffers.hit_counters[i]--;
            }
           
        }	
        #endif
    }

}
