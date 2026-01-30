#include "robot_config.h"

uint8_t g_active_groups = 0; // 激活组数 0~5
light_color_enum global_color = color_red;

uint16_t g_led_ctrl_mask=0; // 击打指示灯掩码
uint8_t target_arm_id_mask=sub_ctrl_id;//激活分控掩码
uint8_t rx_buffer[4]; // UART 接收缓冲区

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
    {WS2812_Update_Task, 30, 0}, // 约33Hz 灯效刷新
    {Hit_Logic_Task,    10, 0}, // 100Hz 击打判定
    {LED_Indicator_Task, 100, 0}, // 10Hz 指示灯刷新
    {UART_Comm_Task, 100, 0}  // 10Hz 通信处理
};

void UART_Comm_Task(void)
{
    // UART 通信处理逻辑
    // 发送击打状态数据
    // 接收控制指令数据
	// 检测击打状态变化
	static uint16_t last_led_ctrl_mask = 0;        
	
    if(target_arm_id_mask&sub_ctrl_id)
    {
        
        if(last_led_ctrl_mask != g_led_ctrl_mask)
        {
            last_led_ctrl_mask = g_led_ctrl_mask;
            // 发送击打状态数据
            static uint8_t tx_buffer[4];
            tx_buffer[0] = PACKET_HEADER; // 起始字节
            tx_buffer[1] = (g_led_ctrl_mask >> 8) & 0xFF; // 高字节
            tx_buffer[2] = g_led_ctrl_mask & 0xFF;        // 低字节
            tx_buffer[3] = tx_buffer[1] ^ tx_buffer[2]; // 简单异或校验
            HAL_UART_Transmit(&huart3, tx_buffer, 4, 10);
        }
    }

}

// 指示灯更新逻辑
void LED_Indicator_Task(void) {
    // 1. 设置颜色切换引脚 (红蓝切换)
    switch (global_color)
    {
    case color_red:
        LED_RED_ENABLE;
        LED_BLUE_DISABLE;
        break;

    case color_blue:
        LED_BLUE_ENABLE;
        LED_RED_DISABLE;
        break;

    case color_off:
    default:
        LED_RED_DISABLE;
        LED_BLUE_DISABLE;
        g_led_ctrl_mask=0x000; // 全部熄灭
        break;
    }

    //记录global_color,只有在从color_off到其他时,才恢复掩码
    static light_color_enum last_color = color_off;
    if(last_color == color_off && global_color != color_off)
    {
        g_led_ctrl_mask=0x3FF; // 全部点亮
    }
    last_color = global_color;

    // 2. 更新10个环的亮灭
    for(int i=0; i<10; i++) {
        HAL_GPIO_WritePin(Ring_LEDs[i].port, Ring_LEDs[i].pin, (g_led_ctrl_mask >> i) & 0x01);
    }

    if(target_arm_id_mask&sub_ctrl_id)
    {
        LED_SHOW_CROSS_PATTERN;
    }
    else
    {
        LED_SHUT_UP_CROSS_PATTERN;
        global_color = color_off;
    }

}


// 初始化任务调度器
void System_Tasks_Init(void) {
    for (int i = 0; i < sizeof(SystemTasks)/sizeof(Task_t); i++) {
        SystemTasks[i].last_run = HAL_GetTick();
    }
    // 启动 ADC DMA 循环采样
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adc_raw, 10);
    // 启动串口中断接收 (假设主控连接在 huart3)
    HAL_UART_Receive_IT(&huart3, rx_buffer, 4);
}

// 运行任务调度器
void System_Tasks_Run(void) {
    static uint32_t sys_tick_last = 0;
    uint32_t now = HAL_GetTick();

    for (int i = 0; i < sizeof(SystemTasks)/sizeof(Task_t); i++) {
        if (now - SystemTasks[i].last_run >= SystemTasks[i].interval) {
            SystemTasks[i].last_run = now;
            SystemTasks[i].task_func();
        }
    }

    if(now - sys_tick_last > 2000)
    {
        sys_tick_last = now;
        //g_active_groups++;
        // if(g_active_groups > MAIN_ARM_STAGES)
        //     g_active_groups = 0;
    }
}

// 串口接收回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        // 1. 校验数据包
        //由于手动计算校验码有点麻烦,先注释掉吧
        #if 0
        if (rx_buffer[0] == PACKET_HEADER && (rx_buffer[1] ^ rx_buffer[2]) == rx_buffer[3]) {
        #else
        if (rx_buffer[0] == PACKET_HEADER) {
        #endif
            // 2. 更新全局状态
            global_color = (light_color_enum)rx_buffer[1];
            g_active_groups = rx_buffer[2];
            
            // 3. 自动判断激活逻辑
            // 如果颜色不为 off，则认为该分控处于激活状态
            if (global_color != color_off) {
                target_arm_id_mask |= sub_ctrl_id; 
            } else {
                target_arm_id_mask &= ~sub_ctrl_id;
            }
        }
        // 4. 重新开启中断接收，准备下一次包
        HAL_UART_Receive_IT(&huart3, rx_buffer, 4);
    }
}