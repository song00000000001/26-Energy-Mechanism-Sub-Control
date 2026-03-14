#pragma once

#include "main.h"

void set_indicator_led_mask(uint16_t new_mask);
void set_indicator_color_blue();
void set_indicator_color_red();
void set_indicator_color_off();
void show_cross_pattern();
void shut_up_cross_pattern();
void update_indicator_leds(void);
void set_indicator_led_mask_and_update(uint16_t new_mask);