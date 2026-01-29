#include "resistive_screen.h"
#include "adc.h"
#include "robot_config.h"

uint16_t g_adc_raw[10]; // DMA 自动填充的原始数据
uint16_t g_hit_counters[10] = {0}; // 击打确认计数器

#define HIT_CONFIRM_COUNT 5        // 连续5次采样超过阈值则认为击打
uint16_t HIT_THRESHOLD=2000;  // ADC 击打判定阈值 (根据实际压力调整)

void ADC_System_Start(void) {
    // 启动 ADC DMA 循环采样
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adc_raw, 10);
}

// 击打判定逻辑 (100Hz 运行，即 10ms 检查一次)
void Hit_Logic_Task(void) {
    for(int i = 0; i < 10; i++) {
        if (g_adc_raw[i] > HIT_THRESHOLD) {
            // 超过阈值，计数器增加
            if (g_hit_counters[i] < 255) g_hit_counters[i]++;
            
            // 如果连续多帧超过阈值，认为该环被击中
            if (g_hit_counters[i] >= HIT_CONFIRM_COUNT) {
                // 灭掉对应环的灯：将掩码对应位清零
                g_led_ctrl_mask &= ~(1 << i); 
            }
        } else {
            #if 1
            // 低于阈值，计数器复位（或缓慢减小以实现消抖）
            g_hit_counters[i] = 0;
            #else
            // 4. 低于阈值则计数器回落
            if (g_hit_counters[i] > 0) {
                g_hit_counters[i]--;
            }
            #endif
            // 5.
            // 如果需要击打一次后灯光一直熄灭，则不需要在这里恢复掩码位
            // 如果需要击打结束后恢复，则在这里 g_led_ctrl_mask |= (1 << i);
        }
    }
}

// 如果您配置了 printf 重定向，可以在调度器中增加此任务
void ADC_Debug_Print(void) {
    // 每隔一段时间打印10路数据，观察空载和击打时的峰值
    /*
    printf("ADC: %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\r\n", 
            g_adc_raw[0], g_adc_raw[1], g_adc_raw[2], g_adc_raw[3], g_adc_raw[4],
            g_adc_raw[5], g_adc_raw[6], g_adc_raw[7], g_adc_raw[8], g_adc_raw[9]);
    */
}