#include "bsp_ws2812.h"

static uint16_t tim_pwm_dma_buff[5][test_num_len] = {0};//PWM DMA数据缓存
static uint8_t Pixel_Buff[WS2312_LED_NUM * 3] = {0};//RGB数据缓存

void Set_Pixel_Color(uint8_t* rgb_buff,uint32_t index,uint8_t R,uint8_t G,uint8_t B)
{
	rgb_buff[(index)*3] = G;
	rgb_buff[(index)*3 + 1] = R;
	rgb_buff[(index)*3 + 2] = B;
}

void Buff_translate(uint8_t* color_buff,uint16_t* dma_row_ptr) //颜色数组转换为码元数组
{   
    uint32_t dat_idx = 0;
	for(uint32_t i = 0;i < (WS2312_LED_NUM*3);i++)
	{
        #if 0
		for(uint8_t k = 0;k < 8;k++)// LSB First: 低位先发
		{
			if ( (color_buff[i] >> k) & 1)
                dma_row_ptr[30 + (i * 8) + k] = WS2312_1bit;
            else 
                color_buff[(i * 8) + k] = WS2312_0bit;
		}
        #else
        for(int8_t k = 7; k >= 0; k--) // MSB First: 高位先发
        {
            if ((color_buff[i] >> k) & 0x01) {
                dma_row_ptr[dat_idx++] = WS2312_1bit; // 50
            } else {
                dma_row_ptr[dat_idx++] = WS2312_0bit; // 29
            }
        }
        #endif
	}
}



void armshow(ligntarm_name_enum num,light_color_enum color)
{
    // 1. 根据颜色枚举填充 RGB 缓存
    switch (color)
    {
    case color_red:
        for(uint8_t i=0;i<WS2312_LED_NUM;i++)
        {	
            Set_Pixel_Color(Pixel_Buff, i, 255, 0, 0);
        }
        break;
    case color_green:
        for(uint8_t i=0;i<WS2312_LED_NUM;i++)
        {	
            Set_Pixel_Color(Pixel_Buff, i, 0, 255, 0);
        }
        break;
    case color_blue:
        for(uint8_t i=0;i<WS2312_LED_NUM;i++)
        {	
            Set_Pixel_Color(Pixel_Buff, i, 0, 0, 255);
        }
        break;
    case color_off: 
    default:
        for(uint8_t i=0;i<WS2312_LED_NUM;i++)
        {	
            Set_Pixel_Color(Pixel_Buff, i, 0, 0, 0);
        }
        break;
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

    HAL_TIM_PWM_Start_DMA(htim, channel, (uint32_t *)target_row, test_num_len);
	HAL_Delay(WS2812_delay);
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