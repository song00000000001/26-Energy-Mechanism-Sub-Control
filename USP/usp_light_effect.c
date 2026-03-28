#include "usp_light_effect.h"
#include "bsp_indicator_led.h"
#include "bsp_ws2812.h"
#include "robot_config.h"

//灯板帧结构体
typedef struct {
    light_color_enum color;   // 红 / 蓝 / 关
    uint16_t ring_mask;       // 10个环灯 bit0~bit9
    uint8_t cross_on;         // 是否显示瞄准图案
} IndicatorFrame_t;
//灯臂帧结构体
//主灯臂灯效枚举,包含流动箭头,阶段亮起的矩形块,全亮和全灭状态
typedef enum {
    MAIN_ARM_EFFECT_OFF = 0,
    MAIN_ARM_EFFECT_FLOW,     // 流动箭头
    MAIN_ARM_EFFECT_STAGE,    // 阶段亮起的矩形块
    MAIN_ARM_EFFECT_FULL,     // 全亮
} MainArmEffect_t;
//副灯臂灯效枚举,包含阶段亮起,全亮和全灭状态
typedef enum {
    SUB_ARM_EFFECT_OFF = 0,
    SUB_ARM_EFFECT_STAGE,     // 阶段亮起的矩形块
    SUB_ARM_EFFECT_FULL,      // 全亮
} SubArmEffect_t;
//灯臂帧结构体
typedef struct {
    MainArmEffect_t main_effect; // 主灯臂效果
    SubArmEffect_t sub_effect;   // 副灯臂效果
    uint8_t group_stage;         // 当前组数阶段 0~5
} ArmFrame_t;
//灯效枚举
typedef enum {
    LIGHT_EFFECT_OFF = 0,          // 全灭
    LIGHT_EFFECT_AIMING,           // 待击打瞄准态
    LIGHT_EFFECT_SMALL_HIT,        // 小符击中后
    LIGHT_EFFECT_BIG_STAGE,        // 大符阶段/非待击打灯臂阶段态
    LIGHT_EFFECT_SUCCESS,          // 激活成功
} LightEffectId_t;

static light_color_enum usp_light_current_color = color_off; // 内部灯效，提供接口更新
static uint8_t usp_light_group_stage = 0; // 当前组数阶段，提供接口更新

static uint16_t index_to_mask(uint8_t led_index);
static void Indicator_Apply(IndicatorFrame_t *ind);
static void WS2812_ApplyFrame(ArmFrame_t *arm_frame);
static void render_off(IndicatorFrame_t *ind,ArmFrame_t *arm_frame);
static void render_aiming(IndicatorFrame_t *ind,ArmFrame_t *arm_frame);
static void render_small_hit(IndicatorFrame_t *ind,ArmFrame_t *arm_frame);
static void render_big_stage(IndicatorFrame_t *ind,ArmFrame_t *arm_frame);
static void render_success(IndicatorFrame_t *ind,ArmFrame_t *arm_frame);
static LightEffectId_t UspLight_SelectEffect(uint8_t effect_id);

void UspLight_SetCurrentColor(uint8_t color_id)
{
    switch (color_id) {
    case color_red:
    case color_blue:
    case color_off:
        usp_light_current_color = (light_color_enum)color_id;
        break;
    default:
        usp_light_current_color = color_off;
        break;
    }
}

void UspLight_SetGroupStage(uint8_t stage)
{
    usp_light_group_stage = stage;
}

void UspLight_Update(uint8_t effect_id)
{
    IndicatorFrame_t ind = {0};
    ArmFrame_t arm_frame = {0};
    
    LightEffectId_t eff = UspLight_SelectEffect(effect_id);

    switch (eff) {
    case LIGHT_EFFECT_AIMING:    render_aiming(&ind, &arm_frame); break;
    case LIGHT_EFFECT_SMALL_HIT: render_small_hit(&ind, &arm_frame); break;
    case LIGHT_EFFECT_BIG_STAGE: render_big_stage(&ind, &arm_frame); break;
    case LIGHT_EFFECT_SUCCESS:   render_success(&ind, &arm_frame); break;
    case LIGHT_EFFECT_OFF:
    default:                     render_off(&ind, &arm_frame); break;
    }

    Indicator_Apply(&ind);
    WS2812_ApplyFrame(&arm_frame);
}

//根据灯索引返回对应的掩码,范围1~10
static uint16_t index_to_mask(uint8_t led_index){
    led_index-=1; // 将1~10转换为0~9,整个项目都使用0~9,只有这里是1~10
    if (led_index < ADC_CHANNELS) {
        return (uint16_t)(1 << led_index);
    }
    return 0;
}

static void Indicator_Apply(IndicatorFrame_t *ind)
{
    //为防止闪烁,先关颜色,再设置图案,最后开颜色
    //先关闭颜色
    set_indicator_color_off();
    
    //设置指示灯状态掩码
    set_indicator_led_mask_and_update(ind->ring_mask);
    if (ind->cross_on) {
        show_cross_pattern();
    } else {
        shut_up_cross_pattern();
    }
    //打开颜色
    switch (ind->color) {
        case color_red:
            set_indicator_color_red();
            break;
        case color_blue:
            set_indicator_color_blue();
            break;
        case color_off:
        default:
            set_indicator_color_off();
            break;
    }
}

static void WS2812_ApplyFrame(ArmFrame_t *arm_frame)
{
    uint8_t r = 0, g = 0, b = 0;
    switch (usp_light_current_color) {
        case color_red: r = 255; break;
        case color_blue: b = 255; break;
        case color_off:
        default: break;
    }
    switch (arm_frame->main_effect)
    {
    case MAIN_ARM_EFFECT_FLOW:
        ws2812_main_arm_flow_effect(r, g, b, arm_frame->group_stage); // 根据当前颜色设置流动箭头颜色
        break;
    case MAIN_ARM_EFFECT_STAGE:
        ws2812_main_arm_stage_effect(r, g, b, arm_frame->group_stage); // 根据当前颜色设置阶段亮起的矩形块颜色
        break;
    case MAIN_ARM_EFFECT_FULL:
        ws2812_main_arm_full_effect(r, g, b); // 根据当前颜色设置主灯臂全亮颜色
        break;
    case MAIN_ARM_EFFECT_OFF:
        ws2812_main_arm_full_effect(0, 0, 0); // 主灯臂全灭
        break;
    default:
        break;
    }

    switch (arm_frame->sub_effect)
    {   
    case SUB_ARM_EFFECT_STAGE:
        ws2812_sub_arm_stage_effect(r, g, b, arm_frame->group_stage); // 根据当前颜色设置副灯臂阶段亮起的矩形块颜色
        break;
    case SUB_ARM_EFFECT_FULL:
        ws2812_sub_arm_full_effect(r, g, b); // 根据当前颜色设置副灯臂全亮颜色
        break;
    case SUB_ARM_EFFECT_OFF:
        ws2812_sub_arm_full_effect(0, 0, 0); // 副灯臂全灭
        break;
    default:
        break;
    }

}


static LightEffectId_t UspLight_SelectEffect(uint8_t effect_id)
{
    // 这里可以根据 effect_id 返回对应的灯效枚举
    // 例如:
    switch (effect_id) {
        case 0: return LIGHT_EFFECT_OFF;
        case 1: return LIGHT_EFFECT_AIMING;
        case 2: return LIGHT_EFFECT_SMALL_HIT;
        case 3: return LIGHT_EFFECT_BIG_STAGE;
        case 4: return LIGHT_EFFECT_SUCCESS;
        default: return LIGHT_EFFECT_OFF;
    }
}

static void render_off(IndicatorFrame_t *ind,ArmFrame_t *arm_frame)
{
    ind->color = color_off;
    ind->cross_on = 0;
    ind->ring_mask = 0x000;   // 全灭
    arm_frame->main_effect = MAIN_ARM_EFFECT_OFF;// 主灯臂全灭
    arm_frame->sub_effect = SUB_ARM_EFFECT_OFF;// 副灯臂全灭
}

static void render_aiming(IndicatorFrame_t *ind,ArmFrame_t *arm_frame)
{
    ind->color = usp_light_current_color;
    ind->cross_on = 1;// 显示瞄准图案
    ind->ring_mask = index_to_mask(1) | index_to_mask(6) | index_to_mask(8);// 只亮第2环，第7环和第9环（1~10）
    arm_frame->group_stage = usp_light_group_stage;
    arm_frame->main_effect = MAIN_ARM_EFFECT_FLOW;// 主灯臂显示流动箭头图案
    arm_frame->sub_effect = SUB_ARM_EFFECT_OFF;// 副灯臂熄灭
}

static void render_small_hit(IndicatorFrame_t *ind,ArmFrame_t *arm_frame)
{
    ind->color = usp_light_current_color;
    ind->cross_on = 0;
    ind->ring_mask = index_to_mask(1);   // 只亮第1环,其他全灭(1~10)
    arm_frame->main_effect = MAIN_ARM_EFFECT_OFF;// 主灯臂熄灭
    arm_frame->sub_effect = SUB_ARM_EFFECT_OFF;// 副灯臂熄灭
}
static void render_big_stage(IndicatorFrame_t *ind,ArmFrame_t *arm_frame)
{
    ind->color = usp_light_current_color;
    ind->cross_on = 0;
    ind->ring_mask = 0x000;   // 灯板全部熄灭
    arm_frame->group_stage = usp_light_group_stage; // 主/侧灯臂跟随当前组数阶段性亮起矩形块
    arm_frame->main_effect = MAIN_ARM_EFFECT_STAGE;// 主灯臂阶段亮起的矩形块
    arm_frame->sub_effect = SUB_ARM_EFFECT_STAGE;// 副灯臂阶段亮起的矩形块
}
static void render_success(IndicatorFrame_t *ind,ArmFrame_t *arm_frame)
{
    ind->color = usp_light_current_color;
    ind->cross_on = 0;
    ind->ring_mask = index_to_mask(8);   // 只亮第8环
    arm_frame->main_effect = MAIN_ARM_EFFECT_FULL;// 主灯臂全亮
    arm_frame->sub_effect = SUB_ARM_EFFECT_FULL;// 副灯臂全亮
}
