#pragma once
		   
#include "main.h"

/* --- 机器人配置宏 --- */
#define sub_ctrl_id 0x05  // 分控标识位

/*
0x 210=33    0010 0001 00000000
0x 220=34    0010 0010 00000000
0x 7F0=2032  0111 1111 00000000    过滤器可以设置为

这样可以过滤掉低8位的ID，只接收高8位为0x21和0x22的ID
具体可以根据实际需求调整
*/

#define PACKET_HEADER 0xAA  //包头标识

#define ADC_CHANNELS        10
#define SAMPLE_INTERVAL_US  30


#define CAN_RECEIVE_ID_BASE 0x210  // CAN 接收包头标识
#define CAN_SEND_ID_BASE 0x220 // CAN 发送包头标识
#define CAN_FILTER_ID_MASK 0x7F0 // CAN 过滤器标识，需要过滤小于210，大于220的ID

// 3ms / 30us = 100个采样点
#define WAVE_BUFF_SIZE      133 
#define PRE_HIT_SAMPLES     33   // 预留约 1ms 的前置数据
#define AFTER_HIT_SAMPLES   33   // 预留约 1ms 的击打后数据


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

/* --- 任务调度器结构体 --- */
typedef struct {
    void (*task_func)(void);
    uint32_t interval;   // 运行间隔 (ms)
    uint32_t last_run;   // 上次运行时间, ms
} Task_t;

//机器人状态结构体
typedef struct {
    uint8_t hit_index;      // 击打索引 0~9
    uint8_t color;   // 当前颜色状态
    uint8_t effect_id;     // 当前灯效ID
    uint8_t group_stage;    // 当前组数阶段
} RobotStatus_t;
extern RobotStatus_t robot_status;


/* --- 函数接口 --- */
void System_Tasks_Init(void);
void System_Tasks_Run(void);
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


