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
#define arm_channel_2 TIM_CHANNEL_4
#define arm_tim2 &htim4
#define arm_channel_3 TIM_CHANNEL_2 
#define arm_channel_4 TIM_CHANNEL_3 

// 4路PWM DMA数据缓存: [0,1,2]主灯臂, [3]左右灯臂
static uint16_t tim_pwm_dma_buff[WS2812_ARM_COUNT][dma_data_len] = {0};//PWM DMA数据缓存
static uint8_t Pixel_Buff[WS2312_LED_NUM * 3] = {0};//RGB数据缓存


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


// DMA 完成回调函数
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim)
{
    // 判定是哪个定时器触发的
    if (htim->Instance == TIM3) {
        // 传输完成后立即停止 DMA
        // 停止顺序：先停通道，如果有必要可以手动把 CCR 清零
        HAL_TIM_PWM_Stop_DMA(htim, arm_channel_1);
        HAL_TIM_PWM_Stop_DMA(htim, arm_channel_2);
        // 强制清零 CCR，防止停止瞬间引脚保持高电平
        __HAL_TIM_SET_COMPARE(htim, arm_channel_1, 0);
        __HAL_TIM_SET_COMPARE(htim, arm_channel_2, 0);
    }
    else if (htim->Instance == TIM4) {
        // 传输完成后立即停止 DMA
        // 停止顺序：先停通道，如果有必要可以手动把 CCR 清零
        HAL_TIM_PWM_Stop_DMA(htim, arm_channel_3);
        HAL_TIM_PWM_Stop_DMA(htim, arm_channel_4);
        // 强制清零 CCR，防止停止瞬间引脚保持高电平
        __HAL_TIM_SET_COMPARE(htim, arm_channel_3, 0);
        __HAL_TIM_SET_COMPARE(htim, arm_channel_4, 0);
    }
}         
/* --- 箭头显示优化配置 --- */
// 如果觉得箭头太小或太稀疏，可以调整这两个值
static uint8_t ARROW_STEP_LEN=2;   // 箭头每一级的灯珠数量（控制箭头大小）
static uint8_t ARROW_GAP=6 ;  // 两个箭头之间的空隙灯珠数（控制间距）

static uint16_t g_flow_offset = 1; // 全局流水偏移量

//由于hit状态不需要流水灯和箭头效果，直接全部点亮即可
void light_arm_fill_all(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t temp_pixels[WS2312_LED_NUM * 3] = {0};

    for (int i = 0; i < WS2312_LED_NUM; i++) 
    {
        temp_pixels[i * 3]     = g; // WS2812 典型为 GRB 顺序
        temp_pixels[i * 3 + 1] = r;
        temp_pixels[i * 3 + 2] = b;
    }
    // 将 RGB 数据转换为 PWM 码元
    Buff_translate(temp_pixels, tim_pwm_dma_buff[0]);
    Buff_translate(temp_pixels, tim_pwm_dma_buff[3]);
    // 4. 非阻塞启动 4 路 DMA 传输
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_1, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂outside
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_2, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂middle
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_3, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂inside
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_4, (uint32_t *)tim_pwm_dma_buff[3], dma_data_len);//左右灯臂
}

void WS2812_Update_Task(void)
{
    OBSERVE_TASK_START(OBSERVE_LED_TASK);

    LED_Indicator_Task();
    // 1. 获取当前的全局颜色 (GRB顺序)
    uint8_t r = 0, g = 0, b = 0;

    switch (robot_status.color)
    {
    case color_red:
        r = 255;
        break;
    case color_blue:
        b = 255;
        break;
    case color_hit_red:
        r = 255;
        light_arm_fill_all(r, g, b);
        return;
    case color_hit_blue:
        b = 255;
        light_arm_fill_all(r, g, b);
        return;
    case color_off:
    default:
        r = 0; g = 0; b = 0;
        break;
    }
    // 2. 计算当前允许亮起的灯珠上限 (1~5组, 每组9颗)
    uint8_t active_limit = robot_status.active_groups * LEDS_PER_STAGE;

    // 3. 更新流动偏移量
    // arrow_period 是一个完整图案的长度
    uint16_t arrow_period = (3 * ARROW_STEP_LEN + ARROW_GAP);
    // 每次进入任务自增偏移。如果想减慢流动速度，可以加一个分频计数器。
    g_flow_offset = (g_flow_offset + 1) % arrow_period;
#if 1
    for (int arm_idx = 0; arm_idx < WS2812_ARM_COUNT; arm_idx++) 
    {
        uint8_t temp_pixels[WS2312_LED_NUM * 3] = {0};

        for (int i = 0; i < WS2312_LED_NUM; i++) 
        {
            // 阶段控制：只在激活范围内计算逻辑
            if (i < active_limit) 
            {
                int is_pixel_on = 0;

                if (arm_idx >= sub_arm_left) {
                    // 副灯臂：保持全亮（矩形填充）
                    is_pixel_on = 1; 
                } 
                else {
                    // 主灯臂：流水箭头逻辑
                    // 修正后的 local_pos 计算：(i - offset) 随时间增加，会让图案向大索引方向（下移）流动
                    // 加上 arrow_period * 10 是为了防止 i - offset 出现负数导致取模出错
                    int local_pos = (i + arrow_period * 10 - g_flow_offset) % arrow_period;

                    // --- 箭头指向修正逻辑 ---
                    // 尖端在最前(Inside)，中间在后(Middle)，两翼最后(Outside) -> 形成 V 字指向下方
                    // 如果发现指向还是反的，请互换下面的 arm_idx 判断条件
                    if (arm_idx == main_arm_inside) { 
                        if (local_pos >= 0 && local_pos < ARROW_STEP_LEN) 
                            is_pixel_on = 1;
                    } 
                    else if (arm_idx == main_arm_middle) {
                        if (local_pos >= ARROW_STEP_LEN && local_pos < 2 * ARROW_STEP_LEN) 
                            is_pixel_on = 1;
                    } 
                    else if (arm_idx == main_arm_outside) {
                        if (local_pos >= 2 * ARROW_STEP_LEN && local_pos < 3 * ARROW_STEP_LEN) 
                            is_pixel_on = 1;
                    }
                }

                if (is_pixel_on) {
                    temp_pixels[i * 3]     = g; // WS2812 典型为 GRB 顺序
                    temp_pixels[i * 3 + 1] = r;
                    temp_pixels[i * 3 + 2] = b;
                }
            }
        }
        // 将 RGB 数据转换为 PWM 码元
        Buff_translate(temp_pixels, tim_pwm_dma_buff[arm_idx]);
    }

    // 4. 非阻塞启动 5 路 DMA 传输
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_1, (uint32_t *)tim_pwm_dma_buff[0], dma_data_len);//主灯臂outside
    HAL_TIM_PWM_Start_DMA(arm_tim1, arm_channel_2, (uint32_t *)tim_pwm_dma_buff[1], dma_data_len);//主灯臂middle
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_3, (uint32_t *)tim_pwm_dma_buff[2], dma_data_len);//主灯臂inside
    HAL_TIM_PWM_Start_DMA(arm_tim2, arm_channel_4, (uint32_t *)tim_pwm_dma_buff[3], dma_data_len);//左右灯臂
#endif
    OBSERVE_TASK_END(OBSERVE_LED_TASK);
}


static void LED_Update(void)
{ //根据组数点亮对应指示灯，有5组，但是有10个灯，所以是间隔点亮，如果是1组，就点亮1，如果是2组，就点亮3，以此类推
    robot_status.led_ctrl_mask=0x000; // 先全部熄灭
    robot_status.led_ctrl_mask |= (1 << ((robot_status.active_groups * 2) - 1)) - 1;
}

// 指示灯更新逻辑
void LED_Indicator_Task(void) {

    // 1. 设置颜色切换引脚 (红蓝切换)
    switch (robot_status.color)
    {
    case color_red:
        LED_RED_ENABLE;
        LED_SHOW_CROSS_PATTERN;
        robot_status.led_ctrl_mask=robot_status.hit_mask; // 保持击打状态指示灯
        break;

    case color_blue:
        LED_BLUE_ENABLE;
        LED_SHOW_CROSS_PATTERN;
        robot_status.led_ctrl_mask=robot_status.hit_mask; // 保持击打状态指示灯
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
        robot_status.led_ctrl_mask=0x000; // 全部熄灭
        break;
    }

    //只有在从off切换到color_red或color_blue时，才会点亮全部指示灯
    static light_color_enum last_color = color_off;
    if (last_color == color_off && (robot_status.color == color_red || robot_status.color == color_blue)) {
        robot_status.led_ctrl_mask = 0x3FF; // 点亮全部指示灯
    }
    last_color = robot_status.color;

    // 2. 更新10个环的亮灭
    for(int i=0; i<10; i++) {
        HAL_GPIO_WritePin(robot_status.Ring_LEDs[i].port, robot_status.Ring_LEDs[i].pin, (robot_status.led_ctrl_mask >> i) & 0x01);
    }
}