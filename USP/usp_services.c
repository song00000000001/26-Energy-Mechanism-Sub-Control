#include "robot_config.h"

uint8_t g_active_groups = 0; // 激活组数 0~5
light_color_enum global_color = color_off;
static uint32_t sys_tick_now = 0;

uint16_t g_led_ctrl_mask=0; // 击打指示灯掩码

// 10个指示环的GPIO端口和引脚配置，目前是GPIOB的4~14号引脚
Indicator_LED_t Ring_LEDs[10] = {
    {GPIOB, GPIO_PIN_4},
    {GPIOB, GPIO_PIN_5},
    {GPIOB, GPIO_PIN_6},
    {GPIOB, GPIO_PIN_7},
    {GPIOB, GPIO_PIN_8},
    {GPIOB, GPIO_PIN_9},
    {GPIOB, GPIO_PIN_10},
    {GPIOB, GPIO_PIN_11},
    {GPIOB, GPIO_PIN_12},
    {GPIOB, GPIO_PIN_13}
};

// usp_services.c
Task_t SystemTasks[] = {
    {ADC_Sampling_Task, 2,  0}, // 500Hz 高频采样
    {WS2812_Update_Task, 30, 0}, // 约33Hz 灯效刷新
    {Hit_Logic_Task,    10, 0}, // 100Hz 击打判定
    {LED_Indicator_Task, 100, 0} // 10Hz 指示灯刷新
};

// ADC 采样逻辑
void ADC_Sampling_Task(void) {

}

// WS2812 灯效更新逻辑
void WS2812_Update_Task(void) {
    arm_show_all();
}

// 击打判定逻辑
void Hit_Logic_Task(void) {

}

// 指示灯更新逻辑
void LED_Indicator_Task(void) {
    // 1. 设置颜色切换引脚 (红蓝切换)
    switch (global_color)
    {
    case color_red:
        HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);
        break;

    case color_blue:
        HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_SET);
        break;

    case color_off:
    default:
        HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);
        break;
    }

    // 2. 更新10个环的亮灭
    for(int i=0; i<10; i++) {
        HAL_GPIO_WritePin(Ring_LEDs[i].port, Ring_LEDs[i].pin, (g_led_ctrl_mask >> i) & 0x01);
    }
}


// 初始化任务调度器
void System_Tasks_Init(void) {
    for (int i = 0; i < sizeof(SystemTasks)/sizeof(Task_t); i++) {
        SystemTasks[i].last_run = HAL_GetTick();
    }
}
// 运行任务调度器
void System_Tasks_Run(void) {
    uint32_t now = HAL_GetTick();
    for (int i = 0; i < sizeof(SystemTasks)/sizeof(Task_t); i++) {
        if (now - SystemTasks[i].last_run >= SystemTasks[i].interval) {
            SystemTasks[i].last_run = now;
            SystemTasks[i].task_func();
        }
    }
}

// 主任务函数.测试用
void main_task(void)
{
    static uint32_t sys_tick_last = 0;
    sys_tick_now = HAL_GetTick();
    arm_show_all();
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
    if(sys_tick_now - sys_tick_last > 1000)
    {
        sys_tick_last = sys_tick_now;
        if(global_color == color_off)
            global_color = color_red;
        else if(global_color == color_red)
            global_color = color_blue;
        else
            global_color = color_off;
    }

}