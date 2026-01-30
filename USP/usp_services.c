#include "robot_config.h"

uint8_t g_active_groups = 0; // 激活组数 0~5
light_color_enum global_color = color_red;

uint16_t g_led_ctrl_mask=0; // 击打指示灯掩码


#if use_can_or_uart_comm
CAN_COB CAN_TxMsg;
CAN_COB CAN_RxMsg;
uint8_t free_can_mailbox;
#else
uint8_t rx_buffer[4]; // UART 接收缓冲区
#endif


// 10个指示环的GPIO端口和引脚配置，

/*更新：
A8
C7
B15
B14
B13
B12
B11
B10
C5
C4
*/
Indicator_LED_t Ring_LEDs[10] = {
    {GPIOA, GPIO_PIN_8},
    {GPIOC, GPIO_PIN_7},
    {GPIOB, GPIO_PIN_15},
    {GPIOB, GPIO_PIN_14},
    {GPIOB, GPIO_PIN_13},
    {GPIOB, GPIO_PIN_12},
    {GPIOB, GPIO_PIN_11},
    {GPIOB, GPIO_PIN_10},
    {GPIOC, GPIO_PIN_5},
    {GPIOC, GPIO_PIN_4}
};

// usp_services.c
Task_t SystemTasks[] = {
    {WS2812_Update_Task, 30, 0}, // 约33Hz 灯效刷新
    {Hit_Logic_Task,    10, 0}, // 100Hz 击打判定
    {LED_Indicator_Task, 100, 0}, // 10Hz 指示灯刷新
    {Comm_Task, 100, 0}  // 10Hz 通信处理
};

void Comm_Task(void)
{
    // 通信处理逻辑
    // 发送击打状态数据
    // 接收控制指令数据
	// 检测击打状态变化
	static uint16_t last_led_ctrl_mask = 0;        
	
    //分控的设计逻辑是不设计任何逻辑,只负责根据主控控制状态切换显示,然后一直搬移传感器数据.
    //这里的数据即便分控没有被选中，在变化后也应该给主控，好让主控判断是否打错。
    /*--- 1. 检测击打状态变化并发送击打状态数据 ---*/
    if(last_led_ctrl_mask != g_led_ctrl_mask)
    {
        last_led_ctrl_mask = g_led_ctrl_mask;
        #if use_can_or_uart_comm
        // 通过 CAN 发送击打状态
        CAN_TxMsg.IdType = Can_STDID;
        CAN_TxMsg.ID = CAN_PACKET_HEADER+sub_ctrl_id; // 分控 ID 作为低字节
        CAN_TxMsg.DLC = 2;
        CAN_TxMsg.Data[0] = (g_led_ctrl_mask >> 8) & 0xFF; // 高字节
        CAN_TxMsg.Data[1] = g_led_ctrl_mask & 0xFF;        // 低字节

        free_can_mailbox = HAL_CAN_GetTxMailboxesFreeLevel(&hcan);
        /* Avoid the unused warning*/
        UNUSED(&free_can_mailbox);
        /*todo
        song
        测试以上函数是否可用
        以及是否需要等待邮箱空闲
        可以写一个循环等待邮箱空闲的机制
        */

        CANx_SendData(1, &CAN_TxMsg);

        /*todo
        song
        用can总线的话，存在发送失败的可能性，需要做重发机制。
        即类似i2c的ack机制。
        现在先把双向通信的功能做好，再考虑这个机制。
        */
        
        #else
        // 发送击打状态数据
        static uint8_t tx_buffer[4];
        tx_buffer[0] = PACKET_HEADER; // 起始字节
        tx_buffer[1] = (g_led_ctrl_mask >> 8) & 0xFF; // 高字节
        tx_buffer[2] = g_led_ctrl_mask & 0xFF;        // 低字节
        tx_buffer[3] = tx_buffer[1] ^ tx_buffer[2]; // 简单异或校验
        HAL_UART_Transmit(&huart3, tx_buffer, 4, 10);
        #endif
    }

    /*--- 2. 接收控制指令数据 ---*/
    // 1. 校验数据包
    if (CAN_RxMsg.ID == CAN_PACKET_HEADER && CAN_RxMsg.DLC == 2) {
        // 2. 更新全局状态
        global_color = (light_color_enum)CAN_RxMsg.Data[0];
        g_active_groups = CAN_RxMsg.Data[1];
    }
}

static void LED_Update(void)
{ //根据组数点亮对应指示灯，有5组，但是有10个灯，所以是间隔点亮，如果是1组，就点亮1，如果是2组，就点亮3，以此类推
    g_led_ctrl_mask=0x000; // 先全部熄灭
    g_led_ctrl_mask |= (1 << ((g_active_groups * 2) - 1)) - 1;
}

// 指示灯更新逻辑
void LED_Indicator_Task(void) {
    

    // 1. 设置颜色切换引脚 (红蓝切换)
    switch (global_color)
    {
    case color_red:
        LED_RED_ENABLE;
        LED_SHOW_CROSS_PATTERN;
        g_led_ctrl_mask=0x3FF; // 全部点亮
        break;

    case color_blue:
        LED_BLUE_ENABLE;
        LED_SHOW_CROSS_PATTERN;
        g_led_ctrl_mask=0x3FF; // 全部点亮
        break;

    case color_hit_red:
        LED_RED_ENABLE;
        LED_SHUT_UP_CROSS_PATTERN;
        LED_Update();
        break;

    case color_hit_blue:
        LED_BLUE_ENABLE;
        LED_SHUT_UP_CROSS_PATTERN;
        LED_Update();
        break;

    case color_off:
    default:
        LED_RED_DISABLE;
        LED_SHUT_UP_CROSS_PATTERN;
        g_led_ctrl_mask=0x000; // 全部熄灭
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
    // 启动 ADC DMA 循环采样
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)g_adc_raw, 10);
    #if use_can_or_uart_comm
    // can init
    CAN_Init(&hcan, User_CAN1_RxCpltCallback);
    CAN_Filter_Mask_Config(1, CanFilter_0 | CanFifo_0 | Can_STDID, 0x201, 0x700);
    #else
    // 启动串口中断接收 (huart3)
    HAL_UART_Receive_IT(&huart3, rx_buffer, 4);
    #endif
}

// 运行任务调度器
void System_Tasks_Run(void) {
    static uint32_t now = 0;

    now = HAL_GetTick();   
    for (int i = 0; i < sizeof(SystemTasks)/sizeof(Task_t); i++) {
        if (now - SystemTasks[i].last_run >= SystemTasks[i].interval) {
            SystemTasks[i].last_run = now;
            SystemTasks[i].task_func();
        }
    } 
}

#if use_can_or_uart_comm
// CAN 接收回调函数

void User_CAN1_RxCpltCallback(CAN_COB *CAN_RxCOB)
{
    //拷贝接收到的数据
    memcpy(&CAN_RxMsg, CAN_RxCOB, sizeof(CAN_COB));
}

#else
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
#endif


