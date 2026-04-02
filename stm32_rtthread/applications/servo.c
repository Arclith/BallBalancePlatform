#include <stm32f1xx_hal.h>
#include <rtthread.h>

TIM_HandleTypeDef htim2;

/**
 * @brief 初始化 TIM3 为 PWM 模式 (用于控制舵机)(A15 -> port0,B3 -> port1)
 * @param period 周期 (单位通常为脉冲数，对应 20ms)
 * @param pulse 初始占空比 (500~2500 对应 0.5ms~2.5ms)
 */
void servo_init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};

    // 1. 开启时钟
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_AFIO_CLK_ENABLE();

    // 释放 JTAG（PA15/PB3 为 JTAG 引脚）
    __HAL_AFIO_REMAP_SWJ_NOJTAG();
    // TIM2 部分重映射1：CH1->PA15, CH2->PB3, CH3->PA2, CH4->PA3
    __HAL_AFIO_REMAP_TIM2_PARTIAL_1();

    // 2. 配置引脚 (PA15 -> TIM2_CH1)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_15;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // 2. 配置引脚 (PB3 -> TIM2_CH2)
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // 3. 配置定时器基础参数 (产生 50Hz 信号)
    // 频率 = 72MHz / (prescaler + 1) / (period + 1)
    // 50Hz = 72,000,000 / 72 / 20,000
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 72 - 1;             // 72分频，计数频率 1MHz (1us一次)
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 20000 - 1;             // 自动重装载值，20ms 周期
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_PWM_Init(&htim2);

    // 4. 配置 PWM 通道
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 1500;                // 初始位置: 90度 (1.5ms)
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;

    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_2);
    __HAL_TIM_ENABLE_OCxPRELOAD(&htim2, TIM_CHANNEL_2);

    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1);
    __HAL_TIM_ENABLE_OCxPRELOAD(&htim2, TIM_CHANNEL_1);
}

/**
 * @brief 设置舵机角度
 * @param angle -90 ~ 90 度
 */
void servo_set_angle(float angle,uint8_t port){
    angle += 90.0f;
    if (angle < 0.0f) angle = 0.0f;
    if (angle > 180.0f) angle = 180.0f;
    uint32_t pulse = (uint32_t)(500 + (angle / 180.0f) * 2000);
    if(port==0) __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, pulse);
    else if(port==1) __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_2, pulse);
}

void servo_set_angle_smooth(float angle,uint8_t port){

}


void servo_on_off(uint8_t port,uint8_t on_off){
    if(on_off){
        if(port==0) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
        else if(port==1) HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_2);
        return;
    }
    if(port==0) HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_1);
    else if(port==1) HAL_TIM_PWM_Stop(&htim2, TIM_CHANNEL_2);
}

