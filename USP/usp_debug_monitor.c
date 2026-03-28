#include "usp_debug_monitor.h"

debug_status_t debug_status={
    .observe_task = OBSERVE_NONE,
    .tim5_counter = 32,
	.adc_10_send_enable=2 // 0: 不发送, 1: 发送10路原始数据, 2: 只发送击打状态
};

/*
1. 开启uart发送波形
32us的采样周期下，空闲时无超过击打阈值时,总任务100ms周期下空闲时间64ms,持续击打时,空闲时间3.5ms。
31us的采样周期下，空闲53ms，击打时周期延长到105ms，无空闲。
2. uart发送击打环数
32us同
31us下，击打时周期延长到104ms，无空闲
3. 关闭uart发送
32us同
31us下，击打时1.8ms空闲。
*/
/*
测试串口115200发送波形需要549ms，发送环数需要500us。
1152000波特率下发波形需要90.78ms。发环数361us
*/