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


#define WS2812_ARM_COUNT 4     // 灯臂数量
#define WS2312_LED_NUM 45   // 每条灯臂上的 WS2812 LED 数量
#define MAIN_ARM_STAGES     5     // 主灯臂激活段数
#define LEDS_PER_STAGE      9     // 每段包含的灯珠数 (45/5)
#define WS2812_delay 15 // 每次显示完成后的延时，单位 ms
#define LED_COUNT_PER_STRIP 45
#define MAIN_ARM_STAGES     5
#define ROWS_PER_STAGE      9

#define RED_CTRL_PORT    GPIOB
#define RED_CTRL_PIN     GPIO_PIN_0

#define BLUE_CTRL_PORT    GPIOB
#define BLUE_CTRL_PIN     GPIO_PIN_1

#define LED_CROSS_CTRL_PORT GPIOA
#define LED_CROSS_CTRL_PIN GPIO_PIN_9

#define LED_RED_ENABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_SET);  HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);
#define LED_BLUE_ENABLE HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_SET);  HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);
#define LED_RED_DISABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);  HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);

#define LED_SHOW_CROSS_PATTERN HAL_GPIO_WritePin(LED_CROSS_CTRL_PORT, LED_CROSS_CTRL_PIN, GPIO_PIN_SET);
#define LED_SHUT_UP_CROSS_PATTERN HAL_GPIO_WritePin(LED_CROSS_CTRL_PORT, LED_CROSS_CTRL_PIN, GPIO_PIN_RESET);

/* --- 10环电阻屏与指示灯配置 --- */
#define RING_COUNT          10    // 10路ADC与10路指示灯

extern uint16_t HIT_THRESHOLD;  // ADC 击打判定阈值 (根据实际压力调整)

/* WS2812 颜色定义 */
typedef struct {
uint8_t r;
uint8_t g;
uint8_t b;
} Color_t;

/* --- 任务调度器结构体 --- */
typedef struct {
    void (*task_func)(void);
    uint32_t interval;   // 运行间隔 (ms)
    uint32_t last_run;   // 上次运行时间, ms
} Task_t;

// robot_config.h 建议结构
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} Indicator_LED_t;


typedef enum 
{
    color_off = 0,
    color_red,
    color_blue,
    color_hit_red,
    color_hit_blue
}light_color_enum;

typedef enum 
{
    main_arm_outside = 0,
    main_arm_middle,
    main_arm_inside,
    sub_arm_left,
    sub_arm_right
}ligntarm_name_enum;

typedef enum{
    idle = 0,
    small_energy,
    big_energy,
    success
}EnergySystemMode_t;

//检测击打状态转换
typedef enum{
    before_hit=0,
    record_hit,
    after_hit
}HitState_t;

/* --- 全局状态声明 --- */

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
    EnergySystemMode_t energy_state; // 能量状态
    light_color_enum color;
    uint8_t active_groups; // 激活组数 0~5
    uint16_t led_ctrl_mask;    // 击打指示灯掩码
    uint16_t hit_mask;        // 击打状态掩码
    Indicator_LED_t Ring_LEDs[10];
    uint8_t is_blue_team;  // 1: 蓝方, 0: 红方
    bool is_still_in_hit;
    HitState_t hit_state;
} RobotStatus_t;
extern RobotStatus_t robot_status;

//由于我不想频繁切换示波器探头,所以打算复用一两个gpio,然后通过全局变量切换,观察不同任务的执行时间
typedef enum {
    OBSERVE_NONE = 0,
    OBSERVE_COMM_TASK,
    OBSERVE_WS2812_TASK,
    OBSERVE_HIT_LOGIC_TASK,
    OBSERVE_UART_DMA,
    OBSERVE_PWM_DMA,
} ObserveTask_t;

typedef struct {
    ObserveTask_t observe_task;
    uint16_t tim5_counter;
} debug_status_t;
extern debug_status_t debug_status;

//定义宏方便使用
#define OBSERVE_TASK_START(task)  do { \
    if (debug_status.observe_task == task) { \
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_SET); \
    } \
} while(0)
#define OBSERVE_TASK_END(task)  do { \
    if (debug_status.observe_task == task) { \
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_10, GPIO_PIN_RESET); \
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
