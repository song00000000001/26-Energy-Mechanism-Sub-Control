#include "usp_hit_detect.h"


#include "adc.h"

#include "string.h"
#include <stdbool.h>

//检测击打状态转换
typedef enum{
    before_hit=0,
    record_hit,
    after_hit
}HitState_t;

static HitState_t hit_state=before_hit; // 当前击打状态

static WaveCapture_t wave_capture = { .state = WAVE_IDLE };

const WaveCapture_t* Hit_GetWaveCapture(void) {
    return &wave_capture;
}
/*todo
song
这里和通信有耦合,后续考虑通过信号量解耦,并在主循环来观察信号量操作通信
*/

// ADC DMA 目标缓冲区：独立声明以便精确控制对齐
// STM32F103 无 CCM，DMA 只能访问 SRAM；4 字节对齐保证 AHB 总线按字访问效率最优
// 注：STM32F103 Cortex-M3 无数据 Cache，无需 SCB_CleanDCache 操作
static uint16_t adc_raw[ADC_CHANNELS] __attribute__((aligned(4)));

//击打计数器与检测配置结构体
typedef struct {
    uint32_t hit_counters[ADC_CHANNELS]; // 击打确认计数器
    uint16_t HIT_THRESHOLD;  // ADC 击打判定阈值 (根据实际压力调整)
    uint16_t HIT_CONFIRM_COUNT;        // 连续N次采样超过阈值则认为击打
    uint8_t  adc_pin_map[ADC_CHANNELS]; // ADC引脚到指示灯环的映射表
    uint8_t leave_debounce_count; // 离开消抖延时
} ADCBuffers_t;
/*8,9,0，1,2,7,6,5,43,*/
ADCBuffers_t adc_buffers={
    .HIT_THRESHOLD = 150,  // ADC 击打判定阈值 (根据实际压力调整)
    .HIT_CONFIRM_COUNT = 1,        // 连续N次采样超过阈值则认为击打
    .adc_pin_map = {8, 9, 0, 1, 2, 7, 6, 5, 4,3}, // 映射表，根据实际连线调整
    .leave_debounce_count = 1, // 离开消抖延时
};

inline uint8_t Hit_Map_Ring_To_AdcChannel(uint8_t hit_index){
    if(hit_index < ADC_CHANNELS){
        return adc_buffers.adc_pin_map[hit_index];
    }
    else{
        return 0; // 默认返回第0路，实际使用时可以根据需求调整
    }
}

// 初始化代码,将adcbuf提供给dma,并启动adc dma采样
void Hit_Detection_Init(void) {
    if(HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) {
        // 校准失败处理
        Error_Handler();
    }
    // 启动 ADC DMA 循环采样，将数据写入对齐的独立缓冲区 adc_raw
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_raw, ADC_CHANNELS);
}

void Hit_Detection(HitEvent_t *event)
{
    if(event == NULL)
        return;
        
    event->trigger_ptr = wave_capture.trigger_ptr;

    if (wave_capture.state == WAVE_READY_TO_SEND) {
        // 重置捕获状态机
        wave_capture.state = WAVE_IDLE;
        wave_capture.count_after_hit = 0;
         //比较出最大值的环
        uint32_t max_value=0;
        uint8_t max_index=0;
        for(int i = 0; i <ADC_CHANNELS; i++){
            if(adc_buffers.hit_counters[i]>max_value){
                max_value=adc_buffers.hit_counters[i];
                max_index=i;
            }
            adc_buffers.hit_counters[i] = 0;
        }
        // 更新击打掩码，只记录最大值对应的环
        if(max_value > (adc_buffers.HIT_THRESHOLD)*adc_buffers.HIT_CONFIRM_COUNT){
            event->pending = 1;
            event->hit_index = max_index;
            event->adc_pin_map_index = adc_buffers.adc_pin_map[max_index];
        }
        else{
            event->pending = 0;
            event->hit_index = HIT_INVALID_INDEX; // 无效索引
            event->adc_pin_map_index = HIT_INVALID_INDEX; // 无效索引 
        }
        hit_state = before_hit; // 重置状态机准备下一次检测
    }
    else{
        event->pending = 0;
        event->hit_index = HIT_INVALID_INDEX; // 无效索引
        event->adc_pin_map_index = HIT_INVALID_INDEX; // 无效索引
    }
}


// 击打判定逻辑 (31us检查一次)
void Hit_Logic_Task() {
    static uint8_t leave_count = 0; // 离开消抖计数
    static bool is_still_in_hit=false; // 是否仍在击打中
    if (wave_capture.state != WAVE_READY_TO_SEND) {
        // 将当前的10路数据拷贝进环形缓冲
        memcpy(wave_capture.buffer[wave_capture.write_ptr],
               adc_raw, ADC_CHANNELS * sizeof(uint16_t));
               
        // 指针循环移动
        uint16_t last_ptr = wave_capture.write_ptr;
        wave_capture.write_ptr = (wave_capture.write_ptr + 1) % WAVE_BUFF_SIZE;

        // --- 2. 状态机逻辑与触发判定 ---     
        switch (hit_state)
        {
            case before_hit:
                //如果有一个超过阈值,就跳转到记录击打状态
                for(int i = 0; i < ADC_CHANNELS; i++) {
                    // 若有击打计数器累计超过阈值的，认为有击中情况。
                    if (adc_raw[adc_buffers.adc_pin_map[i]] > adc_buffers.HIT_THRESHOLD) {
                        adc_buffers.hit_counters[i] += adc_raw[adc_buffers.adc_pin_map[i]];// 超过阈值，记录adc值累加到计数器
                        hit_state=record_hit;
                        leave_count = 0;
                        // 触发点记录：当前位置即为触发时刻
                        if (wave_capture.state == WAVE_IDLE) {
                            wave_capture.trigger_ptr = last_ptr;
                            wave_capture.state = WAVE_CAPTURING;
                            wave_capture.count_after_hit = 0;
                        }
                        break;
                    }
                }
            break;

            case record_hit:
            {
                //记录击打数据，如果10个都没超过阈值，说明击打结束，跳转到击打后状态
                is_still_in_hit=false;
                for(int i = 0; i < ADC_CHANNELS; i++) {
                    if (adc_raw[adc_buffers.adc_pin_map[i]] > adc_buffers.HIT_THRESHOLD)  {
                        adc_buffers.hit_counters[i] += adc_raw[adc_buffers.adc_pin_map[i]];// 超过阈值，记录adc值累加到计数器
                        is_still_in_hit=true;
                    }
                }
                if(!is_still_in_hit){
                    // 松开消抖,连续7次30us=210us检测到无信号才结束
                    leave_count++;
                    if(leave_count >= adc_buffers.leave_debounce_count){ 
                        hit_state = after_hit;
                    }
                }
                else
                    leave_count = 0;
            }
            break;

            //等待通信任务结算击打状态后，准备下一次击打检测
            case after_hit:
                // 如果处于捕获状态，计数
                if (wave_capture.state == WAVE_CAPTURING) {
                    wave_capture.count_after_hit++;
                    // 录满剩余的窗口
                    if (wave_capture.count_after_hit >= (AFTER_HIT_SAMPLES)) {
                        wave_capture.state = WAVE_READY_TO_SEND;
                    }
                }

            default:
                break;
        }
    }

}

