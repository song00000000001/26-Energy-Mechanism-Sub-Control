#include "usp_light_effect.h"

//灯板帧结构体
typedef struct {
    light_color_enum color;   // 红 / 蓝 / 关
    uint16_t ring_mask;       // 10个环灯 bit0~bit9
    uint8_t cross_on;         // 是否显示瞄准图案
    uint8_t group_stage;       // 当前组数阶段 0~5
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

static light_color_enum usp_light_current_color = color_off; // 内部灯效，提供接口更新
static uint8_t usp_light_group_stage = 0; // 当前组数阶段，提供接口更新

static uint16_t index_to_mask(uint8_t led_index);
static void render_off(IndicatorFrame_t *ind,ArmFrame_t *arm);
static void render_aiming(IndicatorFrame_t *ind,ArmFrame_t *arm);
static void render_small_hit(IndicatorFrame_t *ind,ArmFrame_t *arm);
static void render_big_stage(IndicatorFrame_t *ind,ArmFrame_t *arm);
static void render_success(IndicatorFrame_t *ind,ArmFrame_t *arm);
static LightEffectId_t UspLight_SelectEffect(uint8_t effect_id);

#include "robot_config.h"
//根据灯索引返回对应的掩码,范围1~10
static uint16_t index_to_mask(uint8_t led_index){
    led_index-=1; // 将1~10转换为0~9
    if (led_index < 10) {
        return (uint16_t)(1 << led_index);
    }
    return 0;
}

static void Indicator_Apply(IndicatorFrame_t *ind)
{
    // Implementation for applying indicator frame
}

static void WS2812_ApplyFrame(ArmFrame_t *arm)
{
    // Implementation for applying WS2812 frame
}

void UspLight_SetCurrentColor(uint8_t color_id)
{
    switch (color_id) {
        case 0: usp_light_current_color = color_off; break;
        case 1: usp_light_current_color = color_red; break;
        case 2: usp_light_current_color = color_blue; break;
        default: usp_light_current_color = color_off; break;
    }
}

void UspLight_SetGroupStage(uint8_t stage)
{
    usp_light_group_stage = stage;
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

static void render_off(IndicatorFrame_t *ind,ArmFrame_t *arm)
{
    ind->color = color_off;
    ind->cross_on = 0;
    ind->ring_mask = 0x000;   // 全灭
}

static void render_aiming(IndicatorFrame_t *ind,ArmFrame_t *arm)
{
    ind->color = usp_light_current_color;
    ind->cross_on = 1;
    ind->ring_mask = index_to_mask(1) | index_to_mask(6) | index_to_mask(8);
}

static void render_small_hit(IndicatorFrame_t *ind,ArmFrame_t *arm)
{
    ind->color = usp_light_current_color;
    ind->cross_on = 0;
    ind->ring_mask = index_to_mask(1);   // 只亮第1环
}
static void render_big_stage(IndicatorFrame_t *ind,ArmFrame_t *arm)
{
    ind->color = usp_light_current_color;
    ind->cross_on = 0;
    ind->ring_mask = 0x000;   // 灯板全部熄灭
    ind->group_stage = usp_light_group_stage; // 主/侧灯臂跟随当前组数阶段性亮起矩形块
}
static void render_success(IndicatorFrame_t *ind,ArmFrame_t *arm)
{
    ind->color = usp_light_current_color;
    ind->cross_on = 0;
    ind->ring_mask = index_to_mask(8);   // 只亮第8环
}
typedef struct {
    uint8_t reversed;
} ArmPhysicalCfg_t;
static ArmPhysicalCfg_t g_arm_cfg[ARM_ROLE_COUNT] = {
    [ARM_ROLE_MAIN_OUTSIDE] = {.reversed = 1},
    [ARM_ROLE_MAIN_MIDDLE] = {.reversed = 1},
    [ARM_ROLE_MAIN_INSIDE] = {.reversed = 1},
    [ARM_ROLE_SUB_LEFT] = {.reversed = 0},
    [ARM_ROLE_SUB_RIGHT] = {.reversed = 0},
};
static uint16_t arm_map_index(uint8_t arm, uint16_t logical_idx)
{
    if(arm >= ARM_ROLE_COUNT) return 0; // 越界保护
    if(logical_idx >= WS2312_LED_NUM) return 0; // 越界保护
    if (g_arm_cfg[arm].reversed) {
        return (WS2312_LED_NUM - 1 - logical_idx);
    }
    return logical_idx;
}