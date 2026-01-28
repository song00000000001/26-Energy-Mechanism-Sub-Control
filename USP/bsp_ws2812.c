#include "bsp_ws2812.h"
#include "robot_config.h"
#include "string.h"
//30 + Num * 3 * 8 + 30

#define PWM_DATA_LEN (WS2312_LED_NUM * 24) // 每个WS2812需要24个码元
#define WS2812_RESET_LEN 40 // 定义重置周期数（800KHz 下，1.25us/bit，40个0约 50us）
#define dma_data_len (PWM_DATA_LEN + WS2812_RESET_LEN) // DMA数据总长度

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

// 5路PWM DMA数据缓存: [0,1,2]主灯臂, [3]左灯臂, [4]右灯臂
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

/**
 * @brief 核心图案生成与发送任务 (静态箭头)
 * 该函数适配 5 路 PWM 输出，并根据 g_active_groups 限制亮起范围
 */
void WS2812_Update_Task(void)
{
    // 1. 获取当前的全局颜色 (GRB顺序)
    uint8_t r = 0, g = 0, b = 0;
    if (global_color == color_red) r = 255;
    else if (global_color == color_blue) b = 255;

    // 2. 计算当前允许亮起的灯珠上限 (1~5组, 每组9颗)
    uint8_t active_limit = g_active_groups * LEDS_PER_STAGE;

    // 3. 为5个DMA通道分别准备数据
    for (int arm_idx = 0; arm_idx < 5; arm_idx++) 
    {
        uint8_t temp_pixels[WS2312_LED_NUM * 3] = {0}; // 临时存放该通道的RGB数据

        for (int i = 0; i < WS2312_LED_NUM; i++) 
        {
            // 只有在激活范围内的灯珠才可能亮起
            if (i < active_limit) 
            {
                int is_pixel_on = 0;

                if (arm_idx >= sub_arm_left) {
                    // --- 副灯臂 (左/右) ---
                    // 只要在激活范围内，全部点亮（矩形效果）
                    is_pixel_on = 1;
                } 
                else {
                    // --- 主灯臂 (1-5灯条) ---
                    // 这里利用 i % 3 的余数来实现物理上的箭头形状：
                    // 排数 (i+1): 1, 4, 7... (i%3==0) -> 亮最外侧(PWM1/outside)
                    // 排数 (i+1): 2, 5, 8... (i%3==1) -> 亮两侧(PWM2/middle)
                    // 排数 (i+1): 3, 6, 9... (i%3==2) -> 亮中心(PWM3/inside)
                    if (arm_idx == main_arm_outside && (i % 3 == 0)) is_pixel_on = 1;
                    else if (arm_idx == main_arm_middle  && (i % 3 == 1)) is_pixel_on = 1;
                    else if (arm_idx == main_arm_inside  && (i % 3 == 2)) is_pixel_on = 1;
                }

                if (is_pixel_on) {
                    temp_pixels[i * 3]     = g; // WS2812 协议是 GRB
                    temp_pixels[i * 3 + 1] = r;
                    temp_pixels[i * 3 + 2] = b;
                }
            }
        }
        
        // 4. 将生成的该通道RGB数组转换为DMA所需的PWM占空比数据
        Buff_translate(temp_pixels, tim_pwm_dma_buff[arm_idx]);
    }

    // 5. 统一非阻塞启动 5 路 DMA
    // TIM3 负责主灯臂
    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_1, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);
    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_3, (uint32_t *)tim_pwm_dma_buff[1], dma_data_len);
    HAL_TIM_PWM_Start_DMA(&htim3, TIM_CHANNEL_4, (uint32_t *)tim_pwm_dma_buff[2], dma_data_len);
    
    // TIM4 负责左右灯臂
    HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_1, (uint32_t *)tim_pwm_dma_buff[3], dma_data_len);
    HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_2, (uint32_t *)tim_pwm_dma_buff[4], dma_data_len);
}