# 能量机构分控 (Energy Mechanism Sub-Controller)

基于 **STM32F103RCT6** 的 RoboMaster 能量机构分控程序。负责击打检测、灯效控制以及与主控的 CAN 通信。

---

## 目录

- [功能简介](#功能简介)
- [硬件信息](#硬件信息)
- [引脚分配](#引脚分配)
- [软件架构](#软件架构)
- [通信协议](#通信协议)
- [灯效说明](#灯效说明)
- [调试说明](#调试说明)
- [开发环境](#开发环境)
- [目录结构](#目录结构)

---

## 功能简介

| 功能模块 | 描述 |
|---|---|
| **击打检测** | 10 路 ADC + DMA 连续采样，TIM5 中断 (~1kHz) 触发判定，环形缓冲区记录触发前后波形 |
| **灯效控制** | WS2812 灯带（主灯臂 / 副灯臂）+ 10 路指示环 LED，支持 5 种灯效模式 |
| **CAN 通信** | 以 1 Mbps 接收主控灯效指令，将击打结果上报主控 |
| **串口调试** | USART3 以 921600 baud 向 VOFA+ 上位机发送 10 路 ADC 原始波形或击打状态 |
| **任务调度** | 轻量级轮询调度器，支持多任务定时执行 |

---

## 硬件信息

| 参数 | 值 |
|---|---|
| 主控芯片 | STM32F103RCT6 (LQFP-64) |
| 外部晶振 | 8 MHz |
| 系统时钟 | 72 MHz (HSE × PLL × 6) |
| ADC 时钟 | 12 MHz (APB2 / 6) |
| CAN 波特率 | 1 Mbps |
| UART 波特率 | 921600 bps |
| 固件库 | STM32Cube FW_F1 V1.8.6 |

---

## 引脚分配

### ADC — 击打传感器输入（10 路，DMA 循环搬运）

| 引脚 | ADC 通道 | 对应环数 |
|---|---|---|
| PA0 | ADC1_IN0 | 环 1 |
| PA1 | ADC1_IN1 | 环 2 |
| PA2 | ADC1_IN2 | 环 3 |
| PA3 | ADC1_IN3 | 环 4 |
| PA4 | ADC1_IN4 | 环 5 |
| PA5 | ADC1_IN5 | 环 6 |
| PA6 | ADC1_IN6 | 环 7 |
| PA7 | ADC1_IN7 | 环 8 |
| PC0 | ADC1_IN10 | 环 9 |
| PC1 | ADC1_IN11 | 环 10 |

### 指示环 LED（GPIO 输出）

| 引脚 | 功能 |
|---|---|
| PA8 | 第 1 环 |
| PC7 | 第 2 环 |
| PB12 | 第 3 环 |
| PB13 | 第 4 环 |
| PB14 | 第 5 环 |
| PB15 | 第 6 环 |
| PB11 | 第 7 环 |
| PB10 | 第 8 环 |
| PC5 | 第 9 环 |
| PC4 | 第 10 环 |
| PB0 | 指示颜色（红） |
| PB1 | 指示颜色（蓝） |
| PC8/PC9 | 瞄准图案控制 / 调试观察引脚 (PC8) |

### WS2812 灯带（TIM PWM + DMA）

| 引脚 | 定时器通道 | 功能 |
|---|---|---|
| PC6 | TIM3_CH1 | 主灯臂 — 通道 1 |
| PC9 | TIM3_CH4 | 主灯臂 — 通道 2 |
| PB7 | TIM4_CH2 | 副灯臂 — 通道 1 |
| PB8 | TIM4_CH3 | 副灯臂 — 通道 2 |

### 通信接口

| 引脚 | 功能 |
|---|---|
| PA11 | CAN1_RX |
| PA12 | CAN1_TX |
| PC10 | USART3_TX |
| PC11 | USART3_RX |
| PA13 / PA14 | SWD 调试 |

---

## 软件架构

```
main()
 ├── 外设初始化（HAL 自动生成）
 └── System_Tasks_Init()
      ├── Hit_Detection_Init()    — 初始化 ADC DMA 采样，启动 TIM5 中断
      ├── user_can_init()         — 配置 CAN 过滤器并启动接收
      └── vofa_frame_tail_init()  — 初始化 VOFA+ 帧尾

System_Tasks_Run()               — 主循环轮询调度
 └── Comm_Task() [100 ms]
      ├── 动态调整 TIM5 采样频率
      ├── can_receive_process()   — 接收主控灯效指令，更新 robot_status
      ├── Hit_Detection()         — 从环形缓冲区结算击打事件
      ├── handle_hit_event()
      │    ├── can_send_hit_status()    — CAN 上报击打索引
      │    ├── vofa_send_hit_status()   — VOFA+ 发送原始波形（调试模式 1）
      │    ├── uart_send_hit_status()   — 串口发送击打状态（调试模式 2）
      │    └── UspLight_OnHit()         — 更新灯效
      └── all_light_effect_control_task() — 刷新灯效显示

TIM5 中断 (~1kHz)
 └── Hit_Logic_Task()             — ADC 阈值判定，写入环形波形缓冲区
```

### 击打检测原理

- ADC1 以 DMA 循环模式对 10 路传感器进行连续采样（采样间隔约 30 µs）。
- TIM5 中断每 ~1 kHz 调用 `Hit_Logic_Task()`，将数据写入大小为 133 点的环形缓冲区。
- 触发后保留 33 点前置数据 + 33 点后置数据，状态机流转：`WAVE_IDLE → WAVE_CAPTURING → WAVE_READY_TO_SEND`。

---

## 通信协议

### CAN 配置

| 参数 | 值 |
|---|---|
| 接收 ID 基址 | `0x210` |
| 发送 ID 基址 | `0x220` |
| 过滤器掩码 | `0x7F0`（仅接收 `0x210`~`0x21F` 范围） |
| 包头字节 | `0xAA` |
| 分控标识 | `sub_ctrl_id = 0x01` |

### 主控 → 分控（接收）

主控通过 CAN 发送灯效控制帧，分控解析后更新 `robot_status`：

| 字段 | 含义 |
|---|---|
| `effect_id` | 灯效 ID（0~5） |
| `color` | 颜色（0=灭，1=红，2=蓝） |
| `active_groups` | 当前组数阶段（0~5） |

### 分控 → 主控（发送）

检测到击打时，分控将被击打的环索引（0~9）打包为 CAN 帧发送至主控。

---

## 灯效说明

灯板共 10 个指示环（1~10 环），灯臂分主灯臂（3 路控制 5 条灯带）和副灯臂（1 路控制 2 条灯带），每条灯带 43 颗 WS2812 LED。

| `effect_id` | 名称 | 灯板 | 主灯臂 | 副灯臂 |
|---|---|---|---|---|
| `0` | 全灭 | 全灭 | 全灭 | 全灭 |
| `1` | 瞄准态 | 第 2、7、9 环亮，显示瞄准图案 | 流水箭头图案 | 熄灭 |
| `2` | 小符击中 | 第 1 环亮 | 全亮 | 全亮 |
| `3` | 大符阶段 / 非待击打灯臂 | 全灭 | 随组数阶段性亮起矩形块 | 随组数阶段性亮起矩形块 |
| `4` | 激活成功 | 第 8 环亮 | 全亮 | 全亮 |
| `5`/`6` | 测试模式 | 击中哪环亮哪环 | — | — |

> 组数阶段 0~5 对应 0~45 颗灯珠，每增加一组点亮 9 颗。

---

## 调试说明

调试状态由 `debug_status` 结构体控制：

| 字段 | 说明 |
|---|---|
| `observe_task` | 选择需要通过 PC8 引脚观测执行时间的任务，用示波器测量高电平宽度 |
| `tim5_counter` | 动态调整 TIM5 自动重装载值以改变采样频率 |
| `adc_10_send_enable` | `0`=不发送，`1`=VOFA+ 发送 10 路原始波形，`2`=仅发送击打状态 |

**VOFA+ 连接**：波特率 921600，使用 FireWater 协议，可实时观察击打前后 10 路 ADC 波形。

---

## 开发环境

| 工具 | 版本 |
|---|---|
| STM32CubeMX | 6.15.0 |
| Keil MDK-ARM | V5.32 |
| STM32Cube FW_F1 | V1.8.6 |
| 编译器 | ARM Compiler (AC5/AC6) |

**编译步骤**：

1. 使用 Keil MDK 打开 `MDK-ARM/stm32f103rct6_8Mhz.uvprojx`。
2. 编译工程（`Build`）。
3. 通过 SWD（PA13/PA14）下载到目标板。

若需修改外设配置，打开 `stm32f103rct6_8Mhz.ioc` 使用 STM32CubeMX 重新生成初始化代码，**不要修改** `/* USER CODE BEGIN */` 至 `/* USER CODE END */` 以外的区域。

---

## 目录结构

```
.
├── Core/
│   ├── Inc/                    # HAL 生成头文件（adc.h, can.h, gpio.h 等）
│   └── Src/                    # HAL 生成源文件（main.c, 外设初始化等）
├── Drivers/
│   ├── CMSIS/                  # ARM CMSIS 内核支持库
│   └── STM32F1xx_HAL_Driver/   # STM32F1 HAL 驱动库
├── MDK-ARM/                    # Keil 工程文件及启动文件
├── USP/                        # 用户应用层代码
│   ├── robot_config.h          # 全局配置宏、数据类型、任务接口声明
│   ├── usp_services.c          # 任务调度器、主通信任务、TIM5 回调
│   ├── usp_hit_detect.c/h      # 击打检测逻辑（ADC 阈值判定、环形缓冲）
│   ├── usp_light_effect.c/h    # 灯效状态机
│   ├── bsp_ws2812.c/h          # WS2812 灯带底层驱动
│   ├── bsp_indicator_led.c/h   # 指示环 LED / 瞄准图案驱动
│   ├── comm_protocal.c/h       # CAN & UART 通信协议封装
│   ├── drv_can.c/h             # SCUT-RobotLab 通用 CAN 驱动
│   ├── usp_debug_monitor.h     # 调试观测宏与状态结构体
│   └── srml_std_lib.h          # 标准库依赖
└── stm32f103rct6_8Mhz.ioc     # STM32CubeMX 工程配置文件
```
