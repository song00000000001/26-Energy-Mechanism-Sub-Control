#pragma once
		   
#include "main.h"
#include "bsp_ws2812.h"
#include "resistive_screen.h"
#include "adc.h"
#include "usart.h"
#include "tim.h"
#include "can.h"
#include "drv_can.h"

/* --- 机器人配置宏 --- */
#define use_can_or_uart_comm 1  // 1: 使用 CAN 通信; 0: 使用 UART 通信
#define sub_ctrl_id 0x01  // 分控标识位
#define PACKET_HEADER 0xAA  //包头标识
#define CAN_PACKET_HEADER 0x200  // CAN 通信包头标识

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

#define LED_RED_ENABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_SET);  HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);
#define LED_BLUE_ENABLE HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_SET);  HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);
#define LED_RED_DISABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);  HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);

#define LED_SHOW_CROSS_PATTERN HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
#define LED_SHUT_UP_CROSS_PATTERN HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);

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



/* --- 全局状态声明 --- */
extern light_color_enum global_color;
extern uint8_t g_active_groups; // 激活组数 0~5
extern uint8_t g_is_blue_team;  // 1: 蓝方, 0: 红方
extern uint16_t g_led_ctrl_mask;    // 击打指示灯掩码
extern Indicator_LED_t Ring_LEDs[10];

/* --- 函数接口 --- */
void System_Tasks_Init(void);
void System_Tasks_Run(void);
void LED_Indicator_Task(void);
void Comm_Task(void);


