#include "stm32f1xx_hal.h"

#define ENCODER_PIN_A GPIO_PIN_5
#define ENCODER_PORT_A GPIOB

#define ENCODER_PIN_B GPIO_PIN_4
#define ENCODER_PORT_B GPIOB

TIM_HandleTypeDef htim3;

void encoder_init(void)
{
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();
    __HAL_RCC_TIM3_CLK_ENABLE();

    // 1. 释放 PB4 的 JTAG 引脚占用 (NJTRST)
    __HAL_AFIO_REMAP_SWJ_NOJTAG();

    // 2. TIM3 部分重映射到 PB4, PB5
    __HAL_AFIO_REMAP_TIM3_PARTIAL();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = ENCODER_PIN_A | ENCODER_PIN_B;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT; // 硬件编码器模式会自动接管
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 3. 配置定时器编码器接口
    TIM_Encoder_InitTypeDef sConfig = {0};

    htim3.Instance = TIM3;
    htim3.Init.Prescaler = 0;
    htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim3.Init.Period = 65535;
    htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;

    sConfig.EncoderMode = TIM_ENCODERMODE_TI12; // 硬件采用4倍频 (抗干扰最强)
    sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC1Filter = 10; // 硬件滤波周期

    sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
    sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
    sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
    sConfig.IC2Filter = 10;

    if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
    {
        /* 错误处理 */
    }

    // 4. 启动编码器计数
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_ALL);
}

// 读取计数值
int16_t encoder_read(void)
{
    // 软件除以4，实现“转一格记一次”的一倍频效果
    return (int16_t)__HAL_TIM_GET_COUNTER(&htim3) / 4;
}