#include "resistive_screen.h"
#include "robot_config.h"

ADCBuffers_t adc_buffers={
    .HIT_THRESHOLD = 700,  // ADC 击打判定阈值 (根据实际压力调整)
    .HIT_CONFIRM_COUNT = 2,        // 连续N次采样超过阈值则认为击打
    .adc_pin_map = {9, 4, 8, 7, 6, 0, 1, 2, 3,5}, // 映射表，根据实际连线调整
    .leave_debounce_count = 7, // 离开消抖延时
};

WaveCapture_t wave_capture = { .state = WAVE_IDLE };

// 击打判定逻辑 (100Hz 运行，即 10ms 检查一次)
void Hit_Logic_Task(void) {

    static uint8_t leave_count = 0; // 离开消抖计数
    if (wave_capture.state != WAVE_READY_TO_SEND) {
        // 将当前的10路数据拷贝进环形缓冲
        memcpy(wave_capture.buffer[wave_capture.write_ptr], 
               adc_buffers.adc_raw, ADC_CHANNELS * sizeof(uint16_t));
               
        // 指针循环移动
        uint16_t last_ptr = wave_capture.write_ptr;
        wave_capture.write_ptr = (wave_capture.write_ptr + 1) % WAVE_BUFF_SIZE;

        // --- 2. 状态机逻辑与触发判定 ---     
        switch (robot_status.hit_state)
        {
            case before_hit:
                //如果有一个超过阈值,就跳转到记录击打状态
                for(int i = 0; i < 10; i++) {
                    // 若有击打计数器累计超过阈值的，认为有击中情况。
                    if (adc_buffers.adc_raw[adc_buffers.adc_pin_map[i]] > adc_buffers.HIT_THRESHOLD) {
                        robot_status.hit_state=record_hit;
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
                robot_status.is_still_in_hit=false;
                for(int i = 0; i < 10; i++) {
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
                // 如果处于捕获状态，计数
                if (wave_capture.state == WAVE_CAPTURING) {
                    wave_capture.count_after_hit++;
                    // 录满剩余的窗口
                    if (wave_capture.count_after_hit >= (AFTER_HIT_SAMPLES)) {
                        wave_capture.state = WAVE_READY_TO_SEND;
                    }
                }
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

/*
想过用串口输出10路adc的数据,但是发现数据量太大,串口发送无法做到实时性发送：
adc采集周期在14us左右，数据捕获更新生在定时器中断里，每30us一次。
原始数据量：10 路 ADC，每路 16 位（2 字节），所以单次采样的原始数据大小为 $10 \times 2 = 20$ 字节。
采样频率：$1 / 0.000030 \approx 33,333\text{Hz}$。
每秒净数据量：$33,333 \times 20 = 666,660$ 字节/秒（约 $651\text{KB/s}$）。
UART 协议开销：UART 是异步传输，每发送 8 位（1 字节）数据，至少需要 1 个起始位和 1 个停止位。
也就是说，传输 1 个字节实际上需要占用 10 个位的带宽。
所需物理波特率：$666,660 \text{ 字节/秒} \times 10 \text{ 位/字节} \approx 6,666,600 \text{ bps} \approx \mathbf{6.67\text{Mbps}}$。
2. STM32F1 硬件极限评估USART3 的限制：你使用的是 USART3。
在 STM32F1 系列中，USART3 挂载在 $36\text{MHz}$ 的 APB1 总线上。
其波特率上限计算公式为 $f_{\text{PCLK}} / 16$，即 $36\text{MHz} / 16 = \mathbf{2.25\text{Mbps}}$。
结论：即使你把 USART3 开到极限波特率（2.25Mbps），也只能满足约 $1/3$ 的实时传输需求（需要 6.67Mbps）。
注：即便使用挂在 $72\text{MHz}$ 总线上的 USART1，其极限也只有 $4.5\text{Mbps}$，依然跑不满。

所以我想用非实时，窗口期的方式来发送一段时间的数据：
假设击打时间是600us，那就把检测到击打前1ms，和击打结束后1ms以及击打范围内的数据存到单片机内，
在100ms周期下利用dma统一发出来。

触发式窗口捕获（Triggered Window Capture）”**是分析瞬态信号（如击打、脉冲、故障波形）的标准做法。
1. 方案合理性评估内存可行性（以 STM32F103 为例）：
采样规模：采样周期 $30\mu\text{s}$，总时长 $2.6\text{ms}$（$1\text{ms}$ 前 + $0.6\text{ms}$ 中 + $1\text{ms}$ 后），总采样点数约为 $2600 / 30 \approx 87$ 组。
数据量：每组数据包含 10 路 uint16_t ADC 值（20 字节）。
总数据量 = $87 \times 20 = 1740$ 字节。
结论：F103 通常有 $20\text{KB}$ 以上 RAM，拿出约 $2\text{KB}$ 做缓存完全没有压力。
带宽压力的释放：将实时传输（约 $666\text{KB/s}$，远超 $115200$ 波特率的 $\approx 11.5\text{KB/s}$）转变为非实时传输。
$1740$ 字节在 $115200$ 波特率下发送仅需约 $150\text{ms}$。如果提高到 $921600$ 波特率，仅需 $18\text{ms}$。
逻辑优势：通过预留“击打前 1ms”的数据，你可以清晰地观察到波形的上升沿起点，这对于调整 HIT_THRESHOLD（阈值）至关重要。
2. 建议的实现架构为了实现“击打前 1ms”的记录，必须引入 环形缓冲区（Circular Buffer）。

3. 注意事项与建议数据对齐：通过环形缓冲发出的数据，起始点可能不在索引 0。
建议在数据包头部加入一个特殊的“帧头”（如 0x55 0xAA）和“偏移量”信息，方便上位机解析。
可视化建议：强烈推荐使用 VOFA+ 软件的“FireWater”协议。
你可以按照它要求的格式拼接字符串或原始数据，它能直接在电脑上把这 10 路 ADC 曲线像示波器一样画出来，非常适合调试这种击打算法。
防止二次触发：在数据发送期间（DMA 传输时），建议暂时挂起击打检测逻辑，防止缓冲区被新数据覆盖导致的画面撕裂。

*/

