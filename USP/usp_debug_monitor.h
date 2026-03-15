#pragma once

#include "main.h"

//调试观察结构体,可以通过修改 observe_task 来观察不同任务的执行时间
typedef enum {
    OBSERVE_NONE = 0,
} ObserveTask_t;
//调试状态结构体
typedef struct {
    ObserveTask_t observe_task;// 当前观察的任务
    uint16_t tim5_counter;//tim5重装载值,会在comm任务被写入tim5,可以动态调整定时中断频率.
    uint8_t adc_10_send_enable;// 是否允许发送10路ADC数据到主控,0: 不发送, 1: 发送10路原始数据, 2: 只发送击打状态.默认1
} debug_status_t;

extern debug_status_t debug_status;

#define observe_gpio_port GPIOC
#define observe_gpio_pin GPIO_PIN_8

//观察宏定义,在对应的任务函数前后使用,可以通过示波器监测PC8引脚的高电平时间来观察任务执行时间
#define OBSERVE_TASK_START(task)  do { \
    if (debug_status.observe_task == task) { \
        HAL_GPIO_WritePin(observe_gpio_port, observe_gpio_pin, GPIO_PIN_SET); \
    } \
} while(0)
#define OBSERVE_TASK_END(task)  do { \
    if (debug_status.observe_task == task) { \
        HAL_GPIO_WritePin(observe_gpio_port, observe_gpio_pin, GPIO_PIN_RESET); \
    } \
} while(0)