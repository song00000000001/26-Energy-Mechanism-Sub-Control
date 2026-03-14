#include "robot_config.h"
#include "bsp_indicator_led.h"
#include "bsp_ws2812.h"
#include "usp_light_effect.h"

static void render_off(const RobotStatus_t *st,
                       IndicatorFrame_t *ind,
                       ArmFrame_t *arm);

static void render_aiming(const RobotStatus_t *st,
                          IndicatorFrame_t *ind,
                          ArmFrame_t *arm);

static void render_small_hit(const RobotStatus_t *st,
                             IndicatorFrame_t *ind,
                             ArmFrame_t *arm);

static void render_big_stage(const RobotStatus_t *st,
                             IndicatorFrame_t *ind,
                             ArmFrame_t *arm);

static void render_success(const RobotStatus_t *st,
                           IndicatorFrame_t *ind,
                           ArmFrame_t *arm);