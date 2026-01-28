#include "robot_config.h"

light_color_enum global_color = color_off;
static uint32_t sys_tick_now = 0;

void main_task(void)
{
    static uint32_t sys_tick_last = 0;
    sys_tick_now = HAL_GetTick();
    arm_show_all();
    HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_7);
    if(sys_tick_now - sys_tick_last > 1000)
    {
        sys_tick_last = sys_tick_now;
        if(global_color == color_off)
            global_color = color_red;
        else if(global_color == color_red)
            global_color = color_blue;
        else
            global_color = color_off;
    }
    
}