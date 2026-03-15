#include "usp_debug_monitor.h"

debug_status_t debug_status={
    .observe_task = OBSERVE_NONE,
    .tim5_counter = 31,//目前的负载下,31时主任务100ms周期稳定,空闲时间12ms左右.如果是30,则周期变成123.3ms，没有空闲。
    .adc_10_send_enable=1 // 0: 不发送, 1: 发送10路原始数据, 2: 只发送击打状态
};