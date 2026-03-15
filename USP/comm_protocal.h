#pragma once

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

//初始化
void user_can_init(void);
void vofa_frame_tail_init(void);
//can接收主控的控制指令,更新全局状态
void can_receive_process(uint8_t *light_effect_id, uint8_t *color, uint8_t *active_groups);
//can发送击打状态到主控,参数是被击打的环的索引
void can_send_hit_status(uint8_t hit_index);
//串口发送击打波形到vofa调试电脑
void vofa_send_hit_status(uint8_t hit_index,uint8_t adc_pin_map_index,uint16_t trigger_ptr, uint16_t const buffer[133][10]);
//串口发送击打状态到调试电脑,参数是被击打的环的索引
void uart_send_hit_status(uint8_t hit_index);


#ifdef __cplusplus
}
#endif


/* 
接收控制指令数据
判断击打状态变化,发送击打状态数据
*/

//分控的设计逻辑是不设计任何逻辑,只负责根据主控控制状态切换显示,然后一直搬移传感器数据.
//这里的数据即便分控没有被选中，在变化后也应该给主控，好让主控判断是否打错。

/*todo
song
测试是否需要等待邮箱空闲
可以写一个循环等待邮箱空闲的机制
用can总线的话，存在发送失败的可能性，需要做重发机制。
即类似i2c的ack机制。
现在先把双向通信的功能做好，再考虑这个机制。
*/
