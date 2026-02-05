#include "robot_config.h"

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
RobotStatus_t robot_status={
    .Ring_LEDs={
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
    },
    .color=color_red,
    .active_groups=5,
    .hit_mask=0x3ff,
    .led_ctrl_mask=0x3FF,
};
	
debug_status_t debug_status={
    .observe_task = OBSERVE_COMM_TASK
};
CommBuffers_t comm_buffers={
    .free_can_mailbox=0,
    .can_rx_complete=false,
    .uart_rx_complete=false
};

// usp_services.c
Task_t SystemTasks[] = {
    {WS2812_Update_Task, 50, 0},    
    {Comm_Task, 50, 0}             
};

// 击打判定只有200us的窗口期,所以需要更高频率的检测,放在定时器5中断里执行,考虑放到adc搬运dma完成回调里执行

//通信处理,改为最终总结击打状态，并发送出去,而击打判定只负责增加击打计数
/* 
通信处理逻辑
发送击打状态数据
接收控制指令数据
判断击打状态变化
*/
//分控的设计逻辑是不设计任何逻辑,只负责根据主控控制状态切换显示,然后一直搬移传感器数据.
//这里的数据即便分控没有被选中，在变化后也应该给主控，好让主控判断是否打错。
void Comm_Task(void)
{
    OBSERVE_TASK_START(OBSERVE_COMM_TASK);
    static bool is_hit_detected=false; // 上次检测到的击打状态
    /*--- 1. 判断击打状态变化---*/
    //由于同一时间只记录一个击打状态,如果出现多个环被击打,需要增加算法来判断哪个环的击打更有可能
    //如果有一个超过阈值,就跳过检查
    for(int i = 0; i < 10; i++) {
        // 若有击打计数器累计超过阈值乘以窗口的，认为有击中情况。
        if (adc_buffers.hit_counters[i] >= adc_buffers.HIT_CONFIRM_COUNT*adc_buffers.HIT_THRESHOLD) {
            is_hit_detected=true;
            break;
        }
    }
    //如果检查到击打
    if(is_hit_detected){
        is_hit_detected=false;
        //比较出最大值的环
        uint16_t max_value=0;
        uint8_t max_index=0;
        for(int i = 0; i <10; i++){
            if(adc_buffers.hit_counters[i]>max_value){
                max_value=adc_buffers.hit_counters[i];
                max_index=i;
            }
            adc_buffers.hit_counters[i] = 0;
        }
        // 更新击打掩码，只记录最大值对应的环
        robot_status.hit_mask = 1 << max_index;
    }

    /*--- 2. 发送击打状态数据 ---*/
    if(robot_status.hit_mask != 0x3ff) //如果有击打发生，并且也不是初始状态
    {
        #if 1
        // 通过 CAN 发送击打状态
        comm_buffers.CAN_TxMsg.IdType = Can_STDID;
        comm_buffers.CAN_TxMsg.ID = CAN_SEND_ID_BASE+sub_ctrl_id; // 分控 ID 作为低字节
        comm_buffers.CAN_TxMsg.DLC = 2;
        comm_buffers.CAN_TxMsg.Data[0] = (robot_status.hit_mask >> 8) & 0xFF; // 高字节
        comm_buffers.CAN_TxMsg.Data[1] = robot_status.hit_mask & 0xFF;        // 低字节
        comm_buffers.free_can_mailbox = HAL_CAN_GetTxMailboxesFreeLevel(&hcan);
        /* Avoid the unused warning*/
        UNUSED(&comm_buffers.free_can_mailbox);
        CANx_SendData(1, &comm_buffers.CAN_TxMsg);
        /*todo
        song
        测试是否需要等待邮箱空闲
        可以写一个循环等待邮箱空闲的机制
        用can总线的话，存在发送失败的可能性，需要做重发机制。
        即类似i2c的ack机制。
        现在先把双向通信的功能做好，再考虑这个机制。
        */
		#endif
        #if 0
		// 发送击打状态数据
        comm_buffers.uart_tx_buf[0] = PACKET_HEADER; // 起始字节
        comm_buffers.uart_tx_buf[1] = (robot_status.hit_mask >> 8) & 0xFF; // 高字节
        comm_buffers.uart_tx_buf[2] = robot_status.hit_mask & 0xFF;        // 低字节
        comm_buffers.uart_tx_buf[3] = comm_buffers.uart_tx_buf[1] ^ comm_buffers.uart_tx_buf[2]; // 简单异或校验
        HAL_UART_Transmit(&huart3, comm_buffers.uart_tx_buf, sizeof(comm_buffers.uart_tx_buf), 10);
        #else
        // 发送击打状态数据,用字符形式发送
        // 例如mask中0号被击打,则发送"0",如果是9号被击打,则发送"9"
        comm_buffers.uart_tx_buf[0] = 'S';
        comm_buffers.uart_tx_buf[1] = (char)(__builtin_ctz(robot_status.hit_mask) + '0'); // '0'~'9'
        comm_buffers.uart_tx_buf[2] = '\r';
        comm_buffers.uart_tx_buf[3] = '\n'; 
        HAL_UART_Transmit_DMA(&huart3, comm_buffers.uart_tx_buf, sizeof(comm_buffers.uart_tx_buf));
		#endif
        robot_status.hit_mask=0x3ff;
    }

    /*--- 3. 接收控制指令数据 ---*/
    // 1. 校验数据包
    if(comm_buffers.can_rx_complete)
    {
        comm_buffers.can_rx_complete=false;
        if (comm_buffers.CAN_RxMsg.ID == (CAN_RECEIVE_ID_BASE+sub_ctrl_id) && comm_buffers.CAN_RxMsg.DLC == 3) {
            // 2. 更新全局状态
            robot_status.color = (light_color_enum)comm_buffers.CAN_RxMsg.Data[0];
            robot_status.active_groups = comm_buffers.CAN_RxMsg.Data[1];
            robot_status.energy_state = (EnergySystemMode_t)comm_buffers.CAN_RxMsg.Data[2];
        }
    }
    if(comm_buffers.uart_rx_complete)
    {
        comm_buffers.uart_rx_complete=false;
        // 1. 校验数据包
    //由于手动计算校验码有点麻烦,先注释掉吧
    #if 0
        if (comm_buffers.uart_rx_buf[0] == PACKET_HEADER && (comm_buffers.uart_rx_buf[1] ^ comm_buffers.uart_rx_buf[2]) == comm_buffers.uart_rx_buf[3]) {
    #else
        if (comm_buffers.uart_rx_buf[0] == PACKET_HEADER) {
    #endif
            // 2. 更新全局状态
            robot_status.color = (light_color_enum)comm_buffers.uart_rx_buf[1];
            robot_status.active_groups = comm_buffers.uart_rx_buf[2];
            robot_status.energy_state = (EnergySystemMode_t)comm_buffers.uart_rx_buf[3];
        }
    }   

    OBSERVE_TASK_END(OBSERVE_COMM_TASK);
}



// 初始化任务调度器
void System_Tasks_Init(void) {
    for (int i = 0; i < sizeof(SystemTasks)/sizeof(Task_t); i++) {
        SystemTasks[i].last_run = HAL_GetTick();
    }
    // 启动 ADC DMA 循环采样
    HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_buffers.adc_raw, 10);
    // can init
    CAN_Init(&hcan, User_CAN1_RxCpltCallback);
    CAN_Filter_Mask_Config(1, CanFilter_0 | CanFifo_0 | Can_STDID,CAN_RECEIVE_ID_BASE,CAN_FILTER_ID_MASK);
    // 启动串口中断接收 (huart3)
    HAL_UART_Receive_IT(&huart3, comm_buffers.uart_rx_buf, 4);
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


// CAN 接收回调函数
void User_CAN1_RxCpltCallback(CAN_COB *CAN_RxCOB)
{
    //拷贝接收到的数据
    memcpy(&comm_buffers.CAN_RxMsg, CAN_RxCOB, sizeof(CAN_COB));
    comm_buffers.can_rx_complete=true;
}

// 串口接收回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        memcpy(comm_buffers.uart_rx_buf, huart->pRxBuffPtr, sizeof(comm_buffers.uart_rx_buf));
        comm_buffers.uart_rx_complete = true;
        // 4. 重新开启中断接收，准备下一次包
        HAL_UART_Receive_IT(&huart3, comm_buffers.uart_rx_buf, sizeof(comm_buffers.uart_rx_buf));
    }
}


