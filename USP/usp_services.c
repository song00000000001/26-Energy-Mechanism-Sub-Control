#include "robot_config.h"
#include "bsp_indicator_led.h"

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
    .color=color_red,
    .active_groups=5,
    .led_ctrl_mask=0x3FF,
};
	
debug_status_t debug_status={
    .observe_task = OBSERVE_HIT_LOGIC_TASK,
    .tim5_counter = 31,//目前的负载下,31时主任务100ms周期稳定,空闲时间12ms左右.如果是30,则周期变成123.3ms，没有空闲。
    .adc_10_send_enable=1 // 0: 不发送, 1: 发送10路原始数据, 2: 只发送击打状态
};
CommBuffers_t comm_buffers={
    .free_can_mailbox=0,
    .can_rx_complete=false,
    .uart_rx_complete=false
};

// usp_services.c
Task_t SystemTasks[] = {
    {Comm_Task, 100, 0},   
    //{WS2812_Update_Task, 100, 0}      
};


// 定义发送缓冲区：100帧 * 每帧(40字节数据 + 4字节结尾) = 4400 字节
VofaFrame_t vofa_tx_buf[WAVE_BUFF_SIZE];
void vofa_frame_tail_init(void) {
    for (int i = 0; i < WAVE_BUFF_SIZE; i++) {
        vofa_tx_buf[i].tail[0] = 0x00;
        vofa_tx_buf[i].tail[1] = 0x00;
        vofa_tx_buf[i].tail[2] = 0x80;
        vofa_tx_buf[i].tail[3] = 0x7F;
    }
}
void wave_send_2_uart(void){
     if (wave_capture.state == WAVE_READY_TO_SEND) {
        // 重置捕获状态机
        wave_capture.state = WAVE_IDLE;
        wave_capture.count_after_hit = 0;
         //比较出最大值的环
        uint32_t max_value=0;
        uint8_t max_index=0;
        for(int i = 0; i <10; i++){
            if(adc_buffers.hit_counters[i]>max_value){
                max_value=adc_buffers.hit_counters[i];
                max_index=i;
            }
            adc_buffers.hit_counters[i] = 0;
        }
        // 更新击打掩码，只记录最大值对应的环
        if(max_value > (adc_buffers.HIT_THRESHOLD)*adc_buffers.HIT_CONFIRM_COUNT){
            robot_status.led_ctrl_mask = 0; // 先清除所有指示灯
            robot_status.led_ctrl_mask = 1 << max_index;
            // 通过 CAN 发送击打状态
            comm_buffers.CAN_TxMsg.IdType = Can_STDID;
            comm_buffers.CAN_TxMsg.ID = CAN_SEND_ID_BASE+sub_ctrl_id; // 分控 ID 作为低字节
            comm_buffers.CAN_TxMsg.DLC = 2;
            comm_buffers.CAN_TxMsg.Data[0] = max_index; // 发送被击打的环的索引
            comm_buffers.CAN_TxMsg.Data[1] = 0; // 预留字节
            comm_buffers.free_can_mailbox = HAL_CAN_GetTxMailboxesFreeLevel(&hcan);
            /* Avoid the unused warning*/
            UNUSED(&comm_buffers.free_can_mailbox);
            CANx_SendData(1, &comm_buffers.CAN_TxMsg);
            // 计算起始点 (触发点前 1ms)
            if(debug_status.adc_10_send_enable==1){
                int16_t start_idx = (int16_t)wave_capture.trigger_ptr - PRE_HIT_SAMPLES;
                while (start_idx < 0) start_idx += WAVE_BUFF_SIZE;
                // 填充 VOFA+ 帧
                for (int i = 0; i < WAVE_BUFF_SIZE; i++) {
                    uint16_t curr_idx = (start_idx + i) % WAVE_BUFF_SIZE;
                    
                    // 转换 10 路数据为 float
                    for (int ch = 0; ch < 10; ch++) {
                        vofa_tx_buf[i].fdata[ch] = (float)wave_capture.buffer[curr_idx][adc_buffers.adc_pin_map[ch]];
                    }
                }
                //为方便观察,把最后一组数据改成环数+1的负数乘以100,即-100,-200,...-1000
                for(int ch=0;ch<10;ch++){
                    vofa_tx_buf[WAVE_BUFF_SIZE-1].fdata[ch]=-100*(max_index+1);
                }
                // 使用 DMA 一次性发出 4400 字节,波特率设为921600，这样发送过程大约只需 50ms,实测63ms
                OBSERVE_TASK_START(OBSERVE_UART_DMA);
                HAL_UART_Transmit_DMA(&huart3, (uint8_t*)vofa_tx_buf, sizeof(vofa_tx_buf));
            }
            else if(debug_status.adc_10_send_enable==2){
                // 发送击打状态数据,用字符形式发送
                // 例如mask中0号被击打,则发送"0",如果是9号被击打,则发送"9"
                comm_buffers.uart_tx_buf[0] = '\n'; 
                comm_buffers.uart_tx_buf[1] = 'S';
                comm_buffers.uart_tx_buf[2] = (char)(max_index + '0'); // '0'~'9'
                comm_buffers.uart_tx_buf[3] = '\n';
                HAL_UART_Transmit_DMA(&huart3, comm_buffers.uart_tx_buf, sizeof(comm_buffers.uart_tx_buf));
            }
        }
        robot_status.hit_state=before_hit;
    }

}

//通信处理,改为最终总结击打状态，并发送出去,而击打判定只负责增加击打计数
/* 
通信处理逻辑
发送击打状态数据
接收控制指令数据
判断击打状态变化
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

//总灯效控制任务,只需要输入灯效id，颜色和组数阶段，就能控制对应的灯效了,不需要再区分主副灯臂了,因为主控直接发送的就是最终的灯效状态了

void all_light_effect_control_task(uint8_t effect_id, uint8_t color_id, uint8_t active_groups)
{
    // OBSERVE_TASK_START(OBSERVE_ALL_LIGHT_EFFECT_TASK);
    UspLight_Update(effect_id);
    UspLight_SetCurrentColor(color_id);
    UspLight_SetGroupStage(active_groups);
    // OBSERVE_TASK_END(OBSERVE_ALL_LIGHT_EFFECT_TASK);
}

void Comm_Task(void)
{
    OBSERVE_TASK_START(OBSERVE_COMM_TASK);

    /*todo
    song
    优化成事件驱动的方式,即接收数据后直接处理,而不是等到定时任务来处理,这样可以更快地响应控制指令的变化,同时也能减少不必要的处理。
    现在先实现功能,后续再优化成事件驱动的方式。
    */
    all_light_effect_control_task(robot_status.effect_id, robot_status.color, robot_status.active_groups);
    __HAL_TIM_SET_AUTORELOAD(&htim5, debug_status.tim5_counter); // 定时器5自动重装载值，用于动态调整tim5定时器中断任务周期，以调整adc采样频率，调试时观察cpu负载，debug测试功能，稳定后注释掉。

    wave_send_2_uart();
   
    /*--- 3. 接收控制指令数据 ---*/
    // can接收
    if(comm_buffers.can_rx_complete)
    {
        comm_buffers.can_rx_complete=false;
        if (comm_buffers.CAN_RxMsg.ID == (CAN_RECEIVE_ID_BASE+sub_ctrl_id) && comm_buffers.CAN_RxMsg.DLC == 3) {
            // 2. 更新全局状态
            robot_status.color = (light_color_enum)comm_buffers.CAN_RxMsg.Data[0];
            robot_status.active_groups = comm_buffers.CAN_RxMsg.Data[1];
            //comm_buffers.CAN_RxMsg.Data[2];
        }
    }
    // uart接收
    // if(comm_buffers.uart_rx_complete)
    // {
    //     comm_buffers.uart_rx_complete=false;
    //     // 1. 校验数据包
    //     if (comm_buffers.uart_rx_buf[0] == PACKET_HEADER) {
    //         // 2. 更新全局状态
    //         robot_status.color = (light_color_enum)comm_buffers.uart_rx_buf[1];
    //         robot_status.active_groups = comm_buffers.uart_rx_buf[2];
    //         //comm_buffers.uart_rx_buf[3];
    //     }
    // }   

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
    vofa_frame_tail_init();
}
#if 0
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
#else
void System_Tasks_Run(void) {
    uint32_t now = HAL_GetTick();   
    for (int i = 0; i < sizeof(SystemTasks)/sizeof(Task_t); i++) {
        // 使用减法处理溢出安全
        if (now - SystemTasks[i].last_run >= SystemTasks[i].interval) {
            
            // --- 核心修复：增量累加而非直接赋值 ---
            SystemTasks[i].last_run += SystemTasks[i].interval;

            // 保护机制：如果系统卡死导致时间落后太多，强制对齐当前时间，防止任务连续补发
            if (now - SystemTasks[i].last_run > SystemTasks[i].interval) {
                SystemTasks[i].last_run = now;
            }

            if (SystemTasks[i].task_func != NULL) {
                SystemTasks[i].task_func();
            }
        }
    } 
}
#endif
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
        comm_buffers.uart_rx_complete = true;
        // 4. 重新开启中断接收，准备下一次包
        HAL_UART_Receive_IT(&huart3, comm_buffers.uart_rx_buf, sizeof(comm_buffers.uart_rx_buf));
    }
}

// 串口发送完成回调
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) { // 确认是你的调试串口
        OBSERVE_TASK_END(OBSERVE_UART_DMA);
    }
}
