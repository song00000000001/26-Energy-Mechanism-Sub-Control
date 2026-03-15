#include "robot_config.h"
#include "bsp_indicator_led.h"
#include "usp_hit_detect.h"
#include "usp_debug_monitor.h"
#include "comm_protocal.h"
#include "tim.h"
#include "usp_light_effect.h"

RobotStatus_t robot_status={
    .color=color_red,
    .group_stage=5,
    .hit_index=0,
    .hit_state=before_hit,
    .effect_id=0
};
	


// usp_services.c
Task_t SystemTasks[] = {
    {Comm_Task, 100, 0},   
    //{WS2812_Update_Task, 100, 0}      
};



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
    //OBSERVE_TASK_START(OBSERVE_COMM_TASK);

    /*todo
    song
    优化成事件驱动的方式,即接收数据后直接处理,而不是等到定时任务来处理,这样可以更快地响应控制指令的变化,同时也能减少不必要的处理。
    现在先实现功能,后续再优化成事件驱动的方式。
    */
    //利用全局状态变量来控制灯效,每次接收控制指令后更新全局状态,然后在定时任务中根据全局状态来控制灯效显示。
    all_light_effect_control_task(robot_status.effect_id, robot_status.color, robot_status.group_stage);
    __HAL_TIM_SET_AUTORELOAD(&htim5, debug_status.tim5_counter); // 定时器5自动重装载值，用于动态调整tim5定时器中断任务周期，以调整adc采样频率，调试时观察cpu负载，debug测试功能，稳定后注释掉。

    
    /*--- 判断击打状态变化,发送击打状态数据 ---*/
    // 检测是否发生击打,如果发生,则将击打前后的10路adc采样数据通过串口发送到调试电脑观察波形,同时将击打状态通过can发送给主控。

   
    /*--- 接收控制指令数据 ---*/
    // can接收
    can_receive_process(&robot_status.color, &robot_status.group_stage);

    //OBSERVE_TASK_END(OBSERVE_COMM_TASK);
}

// 初始化任务调度器
void System_Tasks_Init(void) {
    HAL_TIM_Base_Start_IT(&htim5);
    for (int i = 0; i < sizeof(SystemTasks)/sizeof(Task_t); i++) {
        SystemTasks[i].last_run = HAL_GetTick();
    }
    Hit_Detection_Init(); // 初始化击打检测模块
    user_can_init();
    vofa_frame_tail_init(); // 初始化 VOFA+ 帧尾
    // HAL_UART_Receive_IT(&huart3, comm_buffers.uart_rx_buf, 4);    // 启动串口中断接收 (huart3)
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
