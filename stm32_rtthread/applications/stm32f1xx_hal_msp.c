#include "stm32f1xx_hal.h"
#include "shared_drive_init.h"

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
        __HAL_RCC_I2C1_CLK_ENABLE();

        //PB6-SCL,PB7-SDA
        GPIO_InitTypeDef GPIO_InitStruct={0};
        GPIO_InitStruct.Mode=GPIO_MODE_AF_OD;
        GPIO_InitStruct.Pin=GPIO_PIN_6|GPIO_PIN_7;
        GPIO_InitStruct.Pull=GPIO_PULLUP;
        GPIO_InitStruct.Speed=GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        if(my_hi2c1.base.IT) {
        // 配置中断. ERROR 优先级必须高于 EVENT 优先级！这样 NACK 等错误发生时才能可靠触发 callback!
        HAL_NVIC_SetPriority(I2C1_ER_IRQn, 1, 0);
        HAL_NVIC_EnableIRQ(I2C1_ER_IRQn);
        
        HAL_NVIC_SetPriority(I2C1_EV_IRQn, 2, 0);
        HAL_NVIC_EnableIRQ(I2C1_EV_IRQn);
        }
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        HAL_NVIC_DisableIRQ(I2C1_EV_IRQn);
        HAL_NVIC_DisableIRQ(I2C1_ER_IRQn);
    }
}