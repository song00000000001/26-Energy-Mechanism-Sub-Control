#include "resistive_screen.h"
#include "robot_config.h"

ADCBuffers_t adc_buffers={
    .HIT_THRESHOLD = 700,  // ADC 击打判定阈值 (根据实际压力调整)
    .HIT_CONFIRM_COUNT = 2,        // 连续N次采样超过阈值则认为击打
    .adc_pin_map = {9, 4, 8, 7, 6, 0, 1, 2, 3,5}, // 映射表，根据实际连线调整
    .leave_debounce_count = 7, // 离开消抖延时
};


// 击打判定逻辑 (100Hz 运行，即 10ms 检查一次)
void Hit_Logic_Task(void) {

    static uint8_t leave_count = 0; // 离开消抖计数
    
    switch (robot_status.hit_state)
    {
        case before_hit:
            //如果有一个超过阈值,就跳转到记录击打状态
            for(int i = 1; i < 10; i++) {
                // 若有击打计数器累计超过阈值的，认为有击中情况。
                if (adc_buffers.adc_raw[adc_buffers.adc_pin_map[i]] > adc_buffers.HIT_THRESHOLD) {
                    robot_status.hit_state=record_hit;
                    leave_count = 0;
                    break;
                }
            }
        break;

        case record_hit:
        {
            //记录击打数据，如果10个都没超过阈值，说明击打结束，跳转到击打后状态
            robot_status.is_still_in_hit=false;
            for(int i = 1; i < 10; i++) {
                if (adc_buffers.adc_raw[adc_buffers.adc_pin_map[i]] > adc_buffers.HIT_THRESHOLD)  {
                    adc_buffers.hit_counters[i]+=adc_buffers.adc_raw[adc_buffers.adc_pin_map[i]];// 超过阈值，记录adc值累加到计数器
                    robot_status.is_still_in_hit=true;
                }
            }
            if(!robot_status.is_still_in_hit){
                // 松开消抖,连续7次30us=210us检测到无信号才结束
                leave_count++;
                if(leave_count >= adc_buffers.leave_debounce_count){ 
                    robot_status.hit_state = after_hit;
                }
            }
            else
                leave_count = 0;
        }
        break;

        //等待通信任务结算击打状态后，准备下一次击打检测
        case after_hit:
        default:
            break;
    }
    
}
