#include "stm32f1xx_hal.h"
#include <rtthread.h>
#include "shared_drive_init.h"
#include "stdbool.h"





void I2C1_EV_IRQHandler(void)
{
    rt_interrupt_enter();
    HAL_I2C_EV_IRQHandler(&my_hi2c1.hi2c);
    rt_interrupt_leave();
}

void I2C1_ER_IRQHandler(void)
{
    rt_interrupt_enter();
    HAL_I2C_ER_IRQHandler(&my_hi2c1.hi2c);
    rt_interrupt_leave();
}

void DMA1_Channel6_IRQHandler(void)
{
    rt_interrupt_enter();
    HAL_DMA_IRQHandler(&my_hi2c1.hdma_tx);
    rt_interrupt_leave();
}

void DMA1_Channel7_IRQHandler(void)
{
    rt_interrupt_enter();
    HAL_DMA_IRQHandler(&my_hi2c1.hdma_rx);
    rt_interrupt_leave();
}


void USART3_IRQHandler(void)
{
    rt_interrupt_enter();

   HAL_UART_IRQHandler(&my_huart3.huart);

    rt_interrupt_leave();
}