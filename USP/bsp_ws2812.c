#include "robot_config.h"
//30 + Num * 3 * 8 + 30

#define PWM_DATA_LEN (WS2312_LED_NUM * 24)
// 定义重置周期数（800KHz 下，1.25us/bit，40个0约 50us）
#define WS2812_RESET_LEN 40 
#define dma_data_len (PWM_DATA_LEN + WS2812_RESET_LEN)

#define WS2312_0bit 29
#define WS2312_1bit 50

#define arm_tim1 &htim3
#define arm_channel_1 TIM_CHANNEL_1
#define arm_channel_2 TIM_CHANNEL_3
#define arm_channel_3 TIM_CHANNEL_4
#define arm_tim2 &htim4
#define arm_channel_4 TIM_CHANNEL_1
#define arm_channel_5 TIM_CHANNEL_2
#define arm_channel_6 TIM_CHANNEL_3

static uint16_t tim_pwm_dma_buff[5][dma_data_len] = {0};//PWM DMA数据缓存
static uint8_t Pixel_Buff[WS2312_LED_NUM * 3] = {0};//RGB数据缓存

void Set_Pixel_Color(uint32_t index)
{
    uint8_t R = 0, G = 0, B = 0;
    switch (global_color)
    {
    case color_red:
        R = 255;
        break;
    case color_blue:
        B = 255;
        break;
    case color_off: 
    default:
        break;
    }
	Pixel_Buff[(index)*3] = G;
	Pixel_Buff[(index)*3 + 1] = R;
	Pixel_Buff[(index)*3 + 2] = B;
}

void Buff_translate(uint8_t* color_buff,uint16_t* dma_row_ptr) //颜色数组转换为码元数组
{   
    uint32_t dat_idx = 0;
	for(uint32_t i = 0;i < (WS2312_LED_NUM*3);i++)
	{
        for(int8_t k = 7; k >= 0; k--) // MSB First: 高位先发
        {
            if ((color_buff[i] >> k) & 0x01) {
                dma_row_ptr[dat_idx++] = WS2312_1bit; // 50
            } else {
                dma_row_ptr[dat_idx++] = WS2312_0bit; // 29
            }
        }
	}
}

//上色函数，目前只有红蓝纯色，但是在两个颜色下，点亮的位置不同，图案也不同
//全部上同色
static void light_all(void)
{
    for(uint8_t i=0;i<WS2312_LED_NUM;i++)
    {	
        Set_Pixel_Color(i);
    }
}

void armshow(ligntarm_name_enum num)
{
    // 1. 根据颜色枚举填充 RGB 缓存
    switch (num)
    {
    case main_arm_outside:
        light_all();
        break;
    case main_arm_middle:
        light_all();
        break;
    case main_arm_inside:
        light_all();
        break;
    case sub_arm_left:
        light_all();
        break;
    case sub_arm_right:
        light_all();
        break;
    default:
        return;
    }
    // 2. 获取当前要操作的行地址
    uint16_t* target_row = tim_pwm_dma_buff[num];
    
    // 3. 转换数据
    Buff_translate(Pixel_Buff, target_row);

    // 4. 启动 DMA 传输
    // 直接传递 target_row 指针并强转，避免了 2D 数组偏移计算错误
    TIM_HandleTypeDef* htim = (num < 3) ? &htim3 : &htim4;
    uint32_t channel;
    // 简单的通道映射映射逻辑
    if(num == main_arm_outside) channel = TIM_CHANNEL_1;
    else if(num == main_arm_middle) channel = TIM_CHANNEL_3;
    else if(num == main_arm_inside) channel = TIM_CHANNEL_4;
    else if(num == sub_arm_left)    channel = TIM_CHANNEL_1; // TIM4
    else                            channel = TIM_CHANNEL_2; // TIM4

    HAL_TIM_PWM_Start_DMA(htim, channel, (uint32_t *)target_row, dma_data_len);
	
    /*todo
    song
    这里的延时后续优化为非阻塞方式，比如使用定时器中断或者状态机
    目前为了简化代码逻辑，使用了阻塞延时
    */
    HAL_Delay(WS2812_delay);
}

void arm_show_all(void)
{
    #if 0
    for (ligntarm_name_enum arm = main_arm_outside; arm <= sub_arm_right; arm++) {
        armshow(arm);
    }
    #else
    armshow(main_arm_outside);
    armshow(main_arm_middle);  
    armshow(main_arm_inside);
    armshow(sub_arm_left);
    armshow(sub_arm_right);
    #endif
}

// DMA 完成回调函数
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    // 判定是哪个定时器触发的
    if (htim->Instance == TIM3) {
        // 传输完成后立即停止 DMA
        // 停止顺序：先停通道，如果有必要可以手动把 CCR 清零
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_3);
        HAL_TIM_PWM_Stop_DMA(htim,TIM_CHANNEL_4);
        // 强制清零 CCR，防止停止瞬间引脚保持高电平
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, 0);
		__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_4, 0);
    }
    else if (htim->Instance == TIM4) {
        // 传输完成后立即停止 DMA
        // 停止顺序：先停通道，如果有必要可以手动把 CCR 清零
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
        HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_2);
		HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_3);
        // 强制清零 CCR，防止停止瞬间引脚保持高电平
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);
        __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, 0);
		__HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, 0);
    }
}         