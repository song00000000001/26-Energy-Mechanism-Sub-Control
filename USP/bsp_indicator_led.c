#include "bsp_indicator_led.h"

//灯板led底层控制结构体
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} Indicator_LED_t;

// 10个指示环的GPIO端口和引脚配置，

static Indicator_LED_t Ring_LEDs[10] = {
    {GPIOA, GPIO_PIN_8},//1环
    {GPIOC, GPIO_PIN_7},
    {GPIOB, GPIO_PIN_12},
    {GPIOB, GPIO_PIN_13},
    {GPIOB, GPIO_PIN_14},
    {GPIOB, GPIO_PIN_15},
    {GPIOB, GPIO_PIN_11},
    {GPIOB, GPIO_PIN_10},
    {GPIOC, GPIO_PIN_5},
    {GPIOC, GPIO_PIN_4}
};

//底层端口宏,用于控制灯板颜色和图案
#define RED_CTRL_PORT    GPIOB
#define RED_CTRL_PIN     GPIO_PIN_0
#define BLUE_CTRL_PORT    GPIOB
#define BLUE_CTRL_PIN     GPIO_PIN_1

#define LED_RED_ENABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_SET); 
#define LED_BLUE_ENABLE HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_SET);
#define LED_RED_DISABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET); 
#define LED_BLUE_DISABLE HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET); 

//灯板瞄准图案控制底层宏
#define LED_CROSS_CTRL_PORT GPIOA
#define LED_CROSS_CTRL_PIN GPIO_PIN_9
//灯板指示灯颜色总供电控制宏,可以用来快速切换所有环数led的颜色,或者同时关闭两色

//瞄准图案控制宏
#define LED_SHOW_CROSS_PATTERN HAL_GPIO_WritePin(LED_CROSS_CTRL_PORT, LED_CROSS_CTRL_PIN, GPIO_PIN_SET);
#define LED_SHUT_UP_CROSS_PATTERN HAL_GPIO_WritePin(LED_CROSS_CTRL_PORT, LED_CROSS_CTRL_PIN, GPIO_PIN_RESET);

//底层接口,尽可能简单直接,不涉及任何逻辑判断,上层根据需要调用

// 2. 设置颜色
void set_indicator_color_blue() {
    LED_RED_DISABLE;
    LED_BLUE_ENABLE;
}

void set_indicator_color_red() {
    LED_BLUE_DISABLE;
    LED_RED_ENABLE;
}

void set_indicator_color_off() {
    LED_RED_DISABLE;
    LED_BLUE_DISABLE;
    for(uint8_t i=0;i<10;i++){
        HAL_GPIO_WritePin(Ring_LEDs[i].port, Ring_LEDs[i].pin, GPIO_PIN_RESET); // 关闭所有环
    }
    shut_up_cross_pattern(); // 颜色关闭时也关闭瞄准图案，确保完全熄灭
}
// 3. 设置瞄准图案显示/隐藏
void show_cross_pattern() {
    LED_SHOW_CROSS_PATTERN;
}
void shut_up_cross_pattern() {
    LED_SHUT_UP_CROSS_PATTERN;
}

// 4. 更新10个环的亮灭状态,根据当前的indicator_mask来控制每个环的GPIO输出
void update_indicator_leds(uint16_t new_mask) {

    new_mask &= 0x3FF; // 只保留低10位
    // 2. 更新10个环的亮灭
    for(int i=0; i<10; i++) {
        HAL_GPIO_WritePin(Ring_LEDs[i].port, Ring_LEDs[i].pin, (new_mask >> i) & 0x01);
    }
}
