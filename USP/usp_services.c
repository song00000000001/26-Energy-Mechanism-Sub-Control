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
    .effect_id=5
};
	


// usp_services.c
Task_t SystemTasks[] = {
    {Comm_Task, 100, 0},   
    //{WS2812_Update_Task, 100, 0}      
};



//总灯效控制任务,只需要输入灯效id，颜色和组数阶段，就能控制对应的灯效了,不需要再区分主副灯臂了,因为主控直接发送的就是最终的灯效状态了
void all_light_effect_control_task(uint8_t effect_id, uint8_t color_id, uint8_t active_groups)
{
    OBSERVE_TASK_START(OBSERVE_ALL_LIGHT_EFFECT_TASK);

    static uint8_t last_effect_id = 0xFF;

    if (effect_id != last_effect_id) {
        if ((effect_id == 5) || (effect_id == 6) || (last_effect_id == 5) || (last_effect_id == 6)) {
            UspLight_ResetTestState();
        }
        last_effect_id = effect_id;
    }

    // OBSERVE_TASK_START(OBSERVE_ALL_LIGHT_EFFECT_TASK);
    // 先设置灯效id
    UspLight_SetCurrentColor(color_id);
    // 再设置组数阶段
    UspLight_SetGroupStage(active_groups);
    // 最后更新灯效显示
    UspLight_Update(effect_id);
    OBSERVE_TASK_END(OBSERVE_ALL_LIGHT_EFFECT_TASK);
}

static void handle_hit_event(const HitEvent_t *event);

static void handle_hit_event(const HitEvent_t *event){
    if(event->pending){
        uint8_t max_index=event->hit_index;
        if(max_index<10){ // 有效击打索引范围0~9
            // 通过 CAN 发送击打状态到主控
            can_send_hit_status(max_index); 
            // 通过 UART 发送击打状态到调试电脑
            if(debug_status.adc_10_send_enable==1){
                vofa_send_hit_status(max_index,event->trigger_ptr,Hit_GetWaveCapture()->buffer); // 通过 VOFA+ 发送击打状态
            }
            else if(debug_status.adc_10_send_enable==2){
                uart_send_hit_status(max_index); 
            }
            UspLight_OnHit(max_index) ;
        }
    }
}

void Comm_Task(void)
{
    OBSERVE_TASK_START(OBSERVE_COMM_TASK);
    
    static HitEvent_t hit_event = {0};

    /*--- 动态调整adc采样频率 ---*/
    // 定时器5自动重装载值，用于动态调整tim5定时器中断任务周期，以调整adc采样频率，调试时观察cpu负载，debug测试功能，稳定后注释掉。
    __HAL_TIM_SET_AUTORELOAD(&htim5, debug_status.tim5_counter); 

    /*--- 接收控制指令数据 ---*/
    // can接收
    can_receive_process(&robot_status.effect_id, &robot_status.color, &robot_status.group_stage);

    /*--- 判断击打状态变化,发送击打状态数据 ---*/
    // 检测是否发生击打,如果发生,则将击打前后的10路adc采样数据通过串口发送到调试电脑观察波形,同时将击打状态通过can发送给主控。
    Hit_Detection(&hit_event);
    handle_hit_event(&hit_event);

    /*--- 控制灯效 ---*/
    //利用全局状态变量来控制灯效,每次接收控制指令后更新全局状态,然后在定时任务中根据全局状态来控制灯效显示。
    all_light_effect_control_task(robot_status.effect_id, robot_status.color, robot_status.group_stage);
    
    OBSERVE_TASK_END(OBSERVE_COMM_TASK);
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

//定时器5中断回调
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM5) {
        OBSERVE_TASK_START(OBSERVE_HIT_LOGIC_TASK);
        Hit_Logic_Task();
        OBSERVE_TASK_END(OBSERVE_HIT_LOGIC_TASK);
    }
}