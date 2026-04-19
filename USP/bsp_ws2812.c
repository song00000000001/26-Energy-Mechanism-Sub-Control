#include "bsp_ws2812.h"
#include "string.h"
#include "tim.h"	

//30 + Num * 3 * 8 + 30
#define WS2812_ARM_COUNT    4       // 待控制的ws2812灯条数量,即pwm通道数量,目前设计为主灯臂用3路控5条+副灯臂用1路控2条,共5条,因为主灯臂图案左右对称，所以只需要3路就能控制5条了，剩下一路给副灯臂控制左右2条显示完全相同的矩形块即可。
#define WS2312_LED_NUM      43      // 每条灯臂上的 WS2812 LED 数量,主侧灯臂刚好都是43颗长度。
#define LEDS_PER_STAGE      9       // 每段包含的灯珠数 (45/5)

#define PWM_DATA_LEN (WS2312_LED_NUM * 24) // 每个WS2812需要24个码元
#define WS2812_RESET_LEN 40 // 定义重置周期数（800KHz 下，1.25us/bit，40个0约 50us）
#define dma_data_len (PWM_DATA_LEN + WS2812_RESET_LEN) // DMA数据总长度

#define WS2312_0bit 29
#define WS2312_1bit 50

#define arm_tim1 &htim3
#define arm_channel_1 TIM_CHANNEL_1
#define arm_channel_2 TIM_CHANNEL_4
#define arm_tim2 &htim4
#define arm_channel_3 TIM_CHANNEL_2 
#define arm_channel_4 TIM_CHANNEL_3 

typedef enum 
{
    main_arm_outside = 0,
    main_arm_middle,
    main_arm_inside,
    sub_arm_left,
    sub_arm_right
}ligntarm_name_enum;

// 4路PWM DMA数据缓存: [0,1,2]主灯臂, [3]左右灯臂
static uint16_t tim_pwm_dma_buff[WS2812_ARM_COUNT][dma_data_len] = {0};//PWM DMA数据缓存
static uint8_t temp_pixels[WS2312_LED_NUM * 3] = {0};//由于大量重复使用,单独拿出来作为全局变量,避免频繁在栈上分配过大的数组

void Buff_translate(uint8_t color_buff[],uint16_t* dma_row_ptr) //颜色数组转换为码元数组
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


// DMA 完成回调函数
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    static uint8_t done_mask = 0;

    if (htim->Instance == TIM3) {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_1);
            __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_1, 0);
            done_mask |= (1 << 0);
        }
        else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_4) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_4);
            __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_4, 0);
            done_mask |= (1 << 1);
        }
    }
    else if (htim->Instance == TIM4) {
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_2) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_2);
            __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_2, 0);
            done_mask |= (1 << 2);
        }
        else if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_3) {
            HAL_TIM_PWM_Stop_DMA(htim, TIM_CHANNEL_3);
            __HAL_TIM_SET_COMPARE(htim, TIM_CHANNEL_3, 0);
            done_mask |= (1 << 3);
        }
    }

    // 例如主灯臂三路都发完
    if ((done_mask & 0x07) == 0x07) {
        done_mask &= ~0x07;
        // 主灯臂本帧完成
    }

    // 如果副灯臂也要一起算总帧完成，可改成 done_mask == 0x0F
}     

uint16_t test_delay_max = 1600;//168Mhz主频，周期0.0595us，一个帧1.25us，需要约210个周期，给约8帧稳定。即1600次。
//目前主灯臂middle偶发绿色灯光闪烁，即HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_2, (uint32_t *)tim_pwm_dma_buff[1], dma_data_len);//主灯臂middle。
//而且如果没有延时，该现象出现的很频繁。

//主灯臂流水灯效控制,输入RGB颜色值,根据当前组数阶段性亮起灯珠,并且让箭头图案流动起来
void ws2812_main_arm_flow_effect(uint8_t r, uint8_t g, uint8_t b, uint8_t active_groups)
{
    static uint8_t ARROW_STEP_LEN = 2;    
    static uint8_t ARROW_GAP = 1;        
    static uint16_t g_flow_offset = 1; 

    //刚发现箭头无需跟随组数变化，这里直接赋值
	active_groups = 5;

    uint8_t active_limit = active_groups * LEDS_PER_STAGE;
    uint16_t arrow_period = (3 * ARROW_STEP_LEN + ARROW_GAP);
    
    // 改变偏移增加方向或逻辑即可改变流动感
    g_flow_offset = (g_flow_offset + 1) % arrow_period;

    for (int arm_idx = 0; arm_idx < WS2812_ARM_COUNT - 1; arm_idx++) 
    {
        memset(temp_pixels, 0, sizeof(temp_pixels)); 

        for (int i = 0; i < WS2312_LED_NUM; i++) 
        {
            // --- 反转增长方向 ---
            // 修改判断条件：只有索引在大端的灯珠才亮起
            if (i >= (WS2312_LED_NUM - active_limit)) 
            {
                int is_pixel_on = 0;
                
                // --- 反转流动与形状逻辑 ---
                // 使用 (WS2312_LED_NUM - 1 - i) 代替 i，直接实现镜像映射
                int reverse_i = WS2312_LED_NUM - 1 - i;
                int local_pos = (reverse_i + arrow_period * 10 - g_flow_offset) % arrow_period;

                // --- 反转箭头指向 ---
                // 调整 arm_idx 对应的 local_pos 区间，或直接镜像逻辑
                if (arm_idx == main_arm_inside) { 
                    if (local_pos >= 0 && local_pos < ARROW_STEP_LEN) is_pixel_on = 1;
                } 
                else if (arm_idx == main_arm_middle) {
                    if (local_pos >= ARROW_STEP_LEN && local_pos < 2 * ARROW_STEP_LEN) is_pixel_on = 1;
                } 
                else if (arm_idx == main_arm_outside) {
                    if (local_pos >= 2 * ARROW_STEP_LEN && local_pos < 3 * ARROW_STEP_LEN) is_pixel_on = 1;
                }

                if (is_pixel_on) {
                    temp_pixels[i * 3]     = g; 
                    temp_pixels[i * 3 + 1] = r;
                    temp_pixels[i * 3 + 2] = b;
                }
            }
        }
        Buff_translate(temp_pixels, tim_pwm_dma_buff[arm_idx]);
    }

    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_1, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂outside
    for(uint16_t i=0;i<test_delay_max;i++) __NOP();; // 这里加个小延时，由于hal库问题,连续启动容易造成相位错乱,后发的通道有时会因为第一帧抖动导致异常
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_2, (uint32_t *)tim_pwm_dma_buff[1], dma_data_len);//主灯臂middle
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_3, (uint32_t *)tim_pwm_dma_buff[2], dma_data_len);//主灯臂inside
}

//主灯臂全亮/灭控制,输入RGB颜色值,直接全亮或者全灭
void ws2812_main_arm_full_effect(uint8_t r, uint8_t g, uint8_t b)
{
    memset(temp_pixels, 0, sizeof(temp_pixels)); // 每次更新前清零像素缓存

    for (int i = 0; i < WS2312_LED_NUM; i++) 
    {
        temp_pixels[i * 3]     = g; // WS2812 典型为 GRB 顺序
        temp_pixels[i * 3 + 1] = r;
        temp_pixels[i * 3 + 2] = b;
    }
    // 将 RGB 数据转换为 PWM 码元
    Buff_translate(temp_pixels, tim_pwm_dma_buff[0]);
   
    // 4. 非阻塞启动 4 路 DMA 传输
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_1, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂outside
    for(uint16_t i=0;i<test_delay_max;i++) __NOP();;
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_2, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂middle
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_3, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂inside
}


//主灯臂阶段亮起矩形块控制,输入RGB颜色值和当前组数,根据当前组数阶段性亮起矩形块
void ws2812_main_arm_stage_effect(uint8_t r, uint8_t g, uint8_t b, uint8_t active_groups)
{
    // 1. 计算当前允许亮起的灯珠上限 (1~5组, 每组9颗)
    uint8_t active_limit = active_groups * LEDS_PER_STAGE;

    memset(temp_pixels, 0, sizeof(temp_pixels)); // 每次更新前清零像素缓存

    for (int i = 0; i < WS2312_LED_NUM; i++) 
    {
        // 修改此处的判断逻辑：从末尾开始亮起
        if (i >= (WS2312_LED_NUM - active_limit)) 
        {
            temp_pixels[i * 3]     = g;
            temp_pixels[i * 3 + 1] = r;
            temp_pixels[i * 3 + 2] = b;
        }
    }
    // 将 RGB 数据转换为 PWM 码元
    Buff_translate(temp_pixels, tim_pwm_dma_buff[0]);

    // 4. 非阻塞启动 4 路 DMA 传输
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_1, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂outside
    for(uint16_t i=0;i<test_delay_max;i++) __NOP(); 
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_2, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂middle
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_3, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂inside
}

//副灯臂全亮/灭控制,输入RGB颜色值,直接全亮或者全灭
void ws2812_sub_arm_full_effect(uint8_t r, uint8_t g, uint8_t b)
{
    memset(temp_pixels, 0, sizeof(temp_pixels)); // 每次更新前清零像素缓存

    for (int i = 0; i < WS2312_LED_NUM; i++) 
    {
        temp_pixels[i * 3]     = g; // WS2812 典型为 GRB 顺序
        temp_pixels[i * 3 + 1] = r;
        temp_pixels[i * 3 + 2] = b;
    }
    // 将 RGB 数据转换为 PWM 码元
    Buff_translate(temp_pixels, tim_pwm_dma_buff[3]);
    // 非阻塞启动 DMA 传输
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_4, (uint32_t *)tim_pwm_dma_buff[3], dma_data_len);//左右灯臂
}

//副灯臂阶段亮起矩形块控制,输入RGB颜色值和当前组数,根据当前组数阶段性亮起矩形块
void ws2812_sub_arm_stage_effect(uint8_t r, uint8_t g, uint8_t b, uint8_t active_groups)
{
    // 1. 计算当前允许亮起的灯珠上限 (1~5组, 每组9颗)
    uint8_t active_limit = active_groups * LEDS_PER_STAGE;

    memset(temp_pixels, 0, sizeof(temp_pixels)); // 每次更新前清零像素缓存

    for (int i = 0; i < WS2312_LED_NUM; i++) 
    {
        // 修改此处的判断逻辑：从末尾开始亮起
        if (i >= (WS2312_LED_NUM - active_limit)) 
        {
            temp_pixels[i * 3]     = g; // WS2812 典型为 GRB 顺序
            temp_pixels[i * 3 + 1] = r;
            temp_pixels[i * 3 + 2] = b;
        }
    }
    // 将 RGB 数据转换为 PWM 码元
    Buff_translate(temp_pixels, tim_pwm_dma_buff[3]);
    // 非阻塞启动 DMA 传输
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_4, (uint32_t *)tim_pwm_dma_buff[3], dma_data_len);//左右灯臂
}