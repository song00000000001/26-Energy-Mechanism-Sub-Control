#pragma once
		   
#include "main.h"
#include "bsp_ws2812.h"
#include "resistive_screen.h"
#include "adc.h"
#include "usart.h"
#include "tim.h"
#include "can.h"
#include "drv_can.h"
#include <stdbool.h>
#include "usp_light_effect.h"

/* --- 机器人配置宏 --- */
#define sub_ctrl_id 0x01  // 分控标识位

#define CAN_RECEIVE_ID_BASE 0x210  // CAN 接收包头标识
#define CAN_SEND_ID_BASE 0x220 // CAN 发送包头标识
#define CAN_FILTER_ID_MASK 0x7F0 // CAN 过滤器标识，需要过滤小于210，大于220的ID
/*
0x 210=33    0010 0001 00000000
0x 220=34    0010 0010 00000000
0x 7F0=2032  0111 1111 00000000    过滤器可以设置为

这样可以过滤掉低8位的ID，只接收高8位为0x21和0x22的ID
具体可以根据实际需求调整
*/

#define PACKET_HEADER 0xAA  //包头标识

/* --- 任务调度器结构体 --- */
typedef struct {
    void (*task_func)(void);
    uint32_t interval;   // 运行间隔 (ms)
    uint32_t last_run;   // 上次运行时间, ms
} Task_t;

//通信收发缓冲区结构体
typedef struct {

	uint8_t free_can_mailbox;
	CAN_COB CAN_RxMsg;
    CAN_COB CAN_TxMsg;
	bool can_rx_complete;
	
    uint8_t uart_rx_buf[4]; // UART 接收缓冲区
    uint8_t uart_tx_buf[4]; // UART 发送缓冲区
    bool uart_rx_complete;
} CommBuffers_t;
extern CommBuffers_t comm_buffers;

//adc原始数据缓冲区和击打计数器结构体
typedef struct {
    uint16_t adc_raw[10]; // DMA 自动填充的原始数据
    uint32_t hit_counters[10]; // 击打确认计数器
    uint16_t HIT_THRESHOLD;  // ADC 击打判定阈值 (根据实际压力调整)
    uint16_t HIT_CONFIRM_COUNT;        // 连续N次采样超过阈值则认为击打
    uint8_t  adc_pin_map[10]; // ADC引脚到指示灯环的映射表
    uint8_t leave_debounce_count; // 离开消抖延时
} ADCBuffers_t;
extern ADCBuffers_t adc_buffers;

//机器人状态结构体
typedef struct {
    uint8_t active_groups; // 激活组数 0~5
    uint16_t led_ctrl_mask;    // 击打指示灯掩码
    Indicator_LED_t Ring_LEDs[10];
    uint8_t is_blue_team;  // 1: 蓝方, 0: 红方
    bool is_still_in_hit;
    HitState_t hit_state;
    light_color_enum color;   // 当前颜色状态
} RobotStatus_t;
extern RobotStatus_t robot_status;

//调试观察结构体,可以通过修改 observe_task 来观察不同任务的执行时间
typedef enum {
    OBSERVE_NONE = 0,
    OBSERVE_COMM_TASK,
    OBSERVE_WS2812_TASK,
    OBSERVE_HIT_LOGIC_TASK,
    OBSERVE_UART_DMA,
    OBSERVE_PWM_DMA,
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

/* --- 函数接口 --- */
void System_Tasks_Init(void);
void System_Tasks_Run(void);
void LED_Indicator_Task(void);
void Comm_Task(void);


/*
调整任务优先级
adc dma chan1 = 0; //adc1 dma搬运
tim3/tim4 dma chan3,4,5,6 = 1; //ws2812
tim5 it = 2; //击打计数
uart3 rx it = 3; //uart3接收中断
uart3 tx dma chan2 = 3; //uart3发送dma
can1 rx/tx it= 3; //can1收发中断
*/

//击打状态机设计
/* --- 窗口捕获配置 --- */
#define ADC_CHANNELS        10
#define SAMPLE_INTERVAL_US  30
// 总共捕获5ms的数据（预留充足空间）
// 击打前保留1ms

// 3ms / 30us = 100个采样点
#define WAVE_BUFF_SIZE      133 
#define PRE_HIT_SAMPLES     33   // 预留约 1ms 的前置数据
#define AFTER_HIT_SAMPLES   33   // 预留约 1ms 的击打后数据

// VOFA+ JustFloat 帧结构体
#pragma pack(1) // 确保结构体按1字节对齐，没有填充
typedef struct {
    float fdata[ADC_CHANNELS];
    uint8_t tail[4]; // 帧尾: 0x00 0x00 0x80 0x7F
} VofaFrame_t;
#pragma pack()

typedef enum {
    WAVE_IDLE = 0,      // 循环写入中
    WAVE_CAPTURING,     // 触发中，正在记录击打后数据
    WAVE_READY_TO_SEND  // 记录完成，等待发送
} WaveState_t;

typedef struct {
    uint16_t buffer[WAVE_BUFF_SIZE][ADC_CHANNELS]; // 原始数据缓冲区
    uint16_t write_ptr;        // 当前写入指针
    uint16_t trigger_ptr;      // 触发时刻的指针
    uint16_t count_after_hit;  // 触发后的计数
    WaveState_t state;
} WaveCapture_t;

extern WaveCapture_t wave_capture;
