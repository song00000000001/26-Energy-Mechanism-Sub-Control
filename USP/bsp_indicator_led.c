#include "bsp_indicator_led.h"

//灯板led底层控制结构体
typedef struct {
    GPIO_TypeDef* port;
    uint16_t pin;
} Indicator_LED_t;

static Indicator_LED_t Ring_LEDs[10] = {
    {GPIOA, GPIO_PIN_8},//1环
    {GPIOC, GPIO_PIN_7},
    {GPIOB, GPIO_PIN_15},
    {GPIOB, GPIO_PIN_14},
    {GPIOB, GPIO_PIN_13},
    {GPIOB, GPIO_PIN_12},
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

#define LED_RED_ENABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_SET);  HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);
#define LED_BLUE_ENABLE HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_SET);  HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);
#define LED_RED_DISABLE HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);  HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);
#define LED_BLUE_DISABLE HAL_GPIO_WritePin(BLUE_CTRL_PORT, BLUE_CTRL_PIN, GPIO_PIN_RESET);  HAL_GPIO_WritePin(RED_CTRL_PORT, RED_CTRL_PIN, GPIO_PIN_RESET);

//灯板瞄准图案控制底层宏
#define LED_CROSS_CTRL_PORT GPIOA
#define LED_CROSS_CTRL_PIN GPIO_PIN_9
//灯板指示灯颜色总供电控制宏,可以用来快速切换所有环数led的颜色,或者同时关闭两色

//瞄准图案控制宏
#define LED_SHOW_CROSS_PATTERN HAL_GPIO_WritePin(LED_CROSS_CTRL_PORT, LED_CROSS_CTRL_PIN, GPIO_PIN_SET);
#define LED_SHUT_UP_CROSS_PATTERN HAL_GPIO_WritePin(LED_CROSS_CTRL_PORT, LED_CROSS_CTRL_PIN, GPIO_PIN_RESET);

static uint16_t indicator_mask = 0x000; // 10个环的状态掩码,1表示亮,0表示灭,初始全灭

//底层接口,尽可能简单直接,不涉及任何逻辑判断,上层根据需要调用
// 1. 设置指示灯状态掩码,每位对应一个环,1亮0灭
void set_indicator_led_mask(uint16_t new_mask) {
    indicator_mask = new_mask & 0x3FF; // 只保留低10位
}
// 2. 设置颜色
void set_indicator_color_blue() {
    LED_BLUE_DISABLE;
    LED_RED_ENABLE;
}

void set_indicator_color_red() {
    LED_RED_DISABLE;
    LED_BLUE_ENABLE;
}

void set_indicator_color_off() {
    LED_RED_DISABLE;
    LED_BLUE_DISABLE;
}
// 3. 设置瞄准图案显示/隐藏
void show_cross_pattern() {
    LED_SHOW_CROSS_PATTERN;
}
void shut_up_cross_pattern() {
    LED_SHUT_UP_CROSS_PATTERN;
}
// 4. 更新10个环的亮灭状态,根据当前的indicator_mask来控制每个环的GPIO输出
void update_indicator_leds(void) {
    // 2. 更新10个环的亮灭
    for(int i=0; i<10; i++) {
        HAL_GPIO_WritePin(Ring_LEDs[i].port, Ring_LEDs[i].pin, (indicator_mask >> i) & 0x01);
    }
}
// 额外接口,直接输入掩码来更新状态,相当于上面更新掩码和更新环步骤的组合
void set_indicator_led_mask_and_update(uint16_t new_mask) {
    set_indicator_led_mask(new_mask);
    update_indicator_leds();
}