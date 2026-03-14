#include "usp_light_effect.h"

//灯板帧结构体
typedef struct {
    light_color_enum color;   // 红 / 蓝 / 关
    uint16_t ring_mask;       // 10个环灯 bit0~bit9
    uint8_t cross_on;         // 是否显示瞄准图案
} IndicatorFrame_t;
//灯臂帧结构体
typedef enum {
    ARM_ROLE_MAIN_OUTSIDE = 0,
    ARM_ROLE_MAIN_MIDDLE,
    ARM_ROLE_MAIN_INSIDE,
    ARM_ROLE_SUB_LEFT,
    ARM_ROLE_SUB_RIGHT,
    ARM_ROLE_COUNT
} LightArmRole_t;
//逻辑映射表
typedef struct {
    uint8_t rgb[WS2812_ARM_COUNT][WS2312_LED_NUM][3];
} ArmFrame_t;
//灯效枚举
typedef enum {
    LIGHT_EFFECT_OFF = 0,          // 全灭
    LIGHT_EFFECT_AIMING,           // 待击打瞄准态
    LIGHT_EFFECT_SMALL_HIT,        // 小符击中后
    LIGHT_EFFECT_BIG_STAGE,        // 大符阶段/非待击打灯臂阶段态
    LIGHT_EFFECT_SUCCESS,          // 激活成功
} LightEffectId_t;

static void render_off(IndicatorFrame_t *ind,ArmFrame_t *arm);
static void render_aiming(IndicatorFrame_t *ind,ArmFrame_t *arm);
static void render_small_hit(IndicatorFrame_t *ind,ArmFrame_t *arm);
static void render_big_stage(IndicatorFrame_t *ind,ArmFrame_t *arm);
static void render_success(IndicatorFrame_t *ind,ArmFrame_t *arm);

static void Indicator_Apply(IndicatorFrame_t *ind)
{
    // Implementation for applying indicator frame
}

static void WS2812_ApplyFrame(ArmFrame_t *arm)
{
    // Implementation for applying WS2812 frame
}

static LightEffectId_t UspLight_SelectEffect(effect_id)
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

void UspLight_Update(uint8_t effect_id)
{
    IndicatorFrame_t ind = {0};
    ArmFrame_t arm = {0};

    LightEffectId_t eff = UspLight_SelectEffect(effect_id);

    switch (eff) {
    case LIGHT_EFFECT_AIMING:    render_aiming(&ind, &arm); break;
    case LIGHT_EFFECT_SMALL_HIT: render_small_hit(&ind, &arm); break;
    case LIGHT_EFFECT_BIG_STAGE: render_big_stage(&ind, &arm); break;
    case LIGHT_EFFECT_SUCCESS:   render_success(&ind, &arm); break;
    case LIGHT_EFFECT_OFF:
    default:                     render_off(&ind, &arm); break;
    }

    Indicator_Apply(&ind);
    WS2812_ApplyFrame(&arm);
}

static void render_off(IndicatorFrame_t *ind,ArmFrame_t *arm)
{

}

static void render_aiming(IndicatorFrame_t *ind,ArmFrame_t *arm)
{
    
}

static void render_small_hit(IndicatorFrame_t *ind,ArmFrame_t *arm)
{

}
static void render_big_stage(IndicatorFrame_t *ind,ArmFrame_t *arm)
{

}
static void render_success(IndicatorFrame_t *ind,ArmFrame_t *arm)
{

}
