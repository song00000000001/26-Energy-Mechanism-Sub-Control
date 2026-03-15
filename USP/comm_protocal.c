#include "comm_protocal.h"
#include "drv_can.h"
#include "usp_hit_detect.h"
#include "stdbool.h"
#include "robot_config.h"
#include "usart.h"
#include "can.h"
#include "drv_can.h"

// VOFA+ JustFloat 帧结构体
#pragma pack(1) // 确保结构体按1字节对齐，没有填充
typedef struct {
    float fdata[ADC_CHANNELS];
    uint8_t tail[4]; // 帧尾: 0x00 0x00 0x80 0x7F
} VofaFrame_t;
#pragma pack()

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

static CommBuffers_t comm_buffers={
    .free_can_mailbox=0,
    .can_rx_complete=false,
    .uart_rx_complete=false
};

WaveCapture_t wave_capture = { .state = WAVE_IDLE };

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

//下面的函数实现了击打检测，如果检测到击打，就通过CAN发送击打状态，并根据调试配置选择是否发送10路ADC数据到调试电脑
//目前击打检测和通信耦合较深，后续考虑优化一下，把击打检测和通信分离开来，这样可以更清晰地职责划分，同时也方便后续维护和扩展。
void hit_detection_and_transmission(uint16_t *hit_mask,uint8_t* hit_state)
{
   

}

//can发送接口,只需要发被击打的环的索引就行了
void can_send_hit_status(uint8_t hit_index){
    comm_buffers.CAN_TxMsg.IdType = Can_STDID;
    comm_buffers.CAN_TxMsg.ID = CAN_SEND_ID_BASE+sub_ctrl_id; // 分控 ID 作为低字节
    comm_buffers.CAN_TxMsg.DLC = 2;
    comm_buffers.CAN_TxMsg.Data[0] = hit_index; // 发送被击打的环的索引
    comm_buffers.CAN_TxMsg.Data[1] = 0; // 预留字节
    comm_buffers.free_can_mailbox = HAL_CAN_GetTxMailboxesFreeLevel(&hcan);
    /* Avoid the unused warning*/
    UNUSED(&comm_buffers.free_can_mailbox);
    CANx_SendData(1, &comm_buffers.CAN_TxMsg);
}

//vofa发送接口,只需要一个索引输入方便调试观察
//第二个参数需要输入adc_buffers.adc_pin_map[ch]映射表
void vofa_send_hit_status(uint8_t hit_index,uint8_t adc_pin_map_index){
    // 计算起始点 (触发点前 1ms)
    int16_t start_idx = (int16_t)wave_capture.trigger_ptr - PRE_HIT_SAMPLES;
    while (start_idx < 0) start_idx += WAVE_BUFF_SIZE;
    // 填充 VOFA+ 帧
    for (int i = 0; i < WAVE_BUFF_SIZE; i++) {
        uint16_t curr_idx = (start_idx + i) % WAVE_BUFF_SIZE;
        
        // 转换 10 路数据为 float
        for (int ch = 0; ch < 10; ch++) {
            vofa_tx_buf[i].fdata[ch] = (float)wave_capture.buffer[curr_idx][adc_pin_map_index]; 
        }
    }
    //为方便观察,把第一组和最后一组数据改成环数+1的负数乘以100,即-100,-200,...-1000
    for(int ch=0;ch<10;ch++){
        vofa_tx_buf[0].fdata[ch]=-100*(hit_index+1);
        vofa_tx_buf[WAVE_BUFF_SIZE-1].fdata[ch]=-100*(hit_index+1);
    }
    // 使用 DMA 一次性发出 4400 字节,波特率设为921600，这样发送过程大约只需 50ms,实测63ms
    // 要是用蓝牙模块只有115200的波特率，可能会消耗250ms
    HAL_UART_Transmit_DMA(&huart3, (uint8_t*)vofa_tx_buf, sizeof(vofa_tx_buf));    
}

//串口发送简易接口,发送被击打的环的索引,方便调试观察
void uart_send_hit_status(uint8_t hit_index){
    comm_buffers.uart_tx_buf[0] = '\n'; 
    comm_buffers.uart_tx_buf[1] = 'S';
    comm_buffers.uart_tx_buf[2] = (char)(hit_index + '0'); // '0'~'9'
    comm_buffers.uart_tx_buf[3] = '\n';
    HAL_UART_Transmit_DMA(&huart3, comm_buffers.uart_tx_buf, sizeof(comm_buffers.uart_tx_buf));
}

// 处理can接收数据函数,放在定时任务里,也可以放在回调函数里,放在回调函数里可以更快地响应控制指令的变化,但会增加回调函数的复杂度,现在先放在定时任务里,后续再优化成事件驱动的方式。
// 由于分层解耦的设计，打算修改方式，该函数应该返回一个数据让定时任务来调用，而不是直接在函数里修改全局状态，这样可以更好地控制数据流和状态更新的时机，避免在回调函数里直接修改全局状态可能带来的线程安全问题，同时也能更清晰地看到数据是如何流动和被处理的。

void can_receive_process(uint8_t *color, uint8_t *active_groups){
    /*--- 3. 接收控制指令数据 ---*/
    // can接收
    if(comm_buffers.can_rx_complete)
    {
        comm_buffers.can_rx_complete=false;
        if (comm_buffers.CAN_RxMsg.ID == (CAN_RECEIVE_ID_BASE+sub_ctrl_id) && comm_buffers.CAN_RxMsg.DLC == 3) {
            *color = comm_buffers.CAN_RxMsg.Data[0];
            *active_groups = comm_buffers.CAN_RxMsg.Data[1];
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

// 串口发送完成回调
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) { 
        //OBSERVE_TASK_END(OBSERVE_UART_DMA);
    }
}

// can初始化代码
void user_can_init(void) {
    CAN_Init(&hcan, User_CAN1_RxCpltCallback);
    CAN_Filter_Mask_Config(1, CanFilter_0 | CanFifo_0 | Can_STDID,CAN_RECEIVE_ID_BASE,CAN_FILTER_ID_MASK);
}

//以下是串口接收控制指令的回调函数和处理函数，由于多机不如can方便，现已弃用。
#if 0
// 串口接收回调函数
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART3) {
        comm_buffers.uart_rx_complete = true;
        // 4. 重新开启中断接收，准备下一次包
        HAL_UART_Receive_IT(&huart3, comm_buffers.uart_rx_buf, sizeof(comm_buffers.uart_rx_buf));
    }
}
 // uart接收
if(comm_buffers.uart_rx_complete)
{
    comm_buffers.uart_rx_complete=false;
    // 1. 校验数据包
    if (comm_buffers.uart_rx_buf[0] == PACKET_HEADER) {
        // 2. 更新全局状态
        robot_status.color = (light_color_enum)comm_buffers.uart_rx_buf[1];
        robot_status.active_groups = comm_buffers.uart_rx_buf[2];
        //comm_buffers.uart_rx_buf[3];
    }
}   

#endif

