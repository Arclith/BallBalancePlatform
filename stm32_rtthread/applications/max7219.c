/*默认启用spi2,PB13-CLK,PB15-DIN*/

#include "stm32_hal_legacy.h"
#include "stm32f103xb.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_gpio_ex.h"
#include "stm32f1xx_hal_spi.h"
#include "max7219.h"



SPI_HandleTypeDef hspi2;

/*初始化,参数为CS相连GPIO_PIN*/
void max7219_init(max7219_struct_t *max7219){

    /*开启时钟*/
    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_SPI2_CLK_ENABLE(); 

    /*初始化spi复用gpio*/
    GPIO_InitTypeDef GPIO_struct;
    GPIO_struct.Mode = GPIO_MODE_AF_PP;
    GPIO_struct.Pin = GPIO_PIN_13|GPIO_PIN_15;
    GPIO_struct.Pull = GPIO_NOPULL;
    GPIO_struct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOB, &GPIO_struct);
    
   /*初始化片选引脚*/
    GPIO_struct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_struct.Pin = max7219->CS;
    HAL_GPIO_Init(max7219->GPIOx,&GPIO_struct);
;


    /*初始化spi*/
    hspi2.Instance = SPI2;
    hspi2.Init.Mode = SPI_MODE_MASTER;                  // 主机模式
    hspi2.Init.Direction = SPI_DIRECTION_1LINE;        // 半双工
    hspi2.Init.DataSize = SPI_DATASIZE_8BIT;            // 8位
    hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;          // 空闲低电平
    hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;              // 第1个边沿采样
    hspi2.Init.NSS = SPI_NSS_SOFT;                      // 软件控制片选
    hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;
    hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
    hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
    hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
    hspi2.Init.CRCPolynomial = 10;
    HAL_SPI_Init(&hspi2);

}

/*max7219写数据*/
void max7219_write(max7219_struct_t *max7219,uint8_t reg,uint8_t val){
     uint8_t pdata[2];
    pdata[0]=reg;
    pdata[1]=val;
    HAL_GPIO_WritePin(max7219->GPIOx,max7219->CS,GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi2,  pdata,  2,  HAL_MAX_DELAY);
    while(__HAL_SPI_GET_FLAG(&hspi2, SPI_FLAG_BSY) == SET);
    HAL_GPIO_WritePin(max7219->GPIOx,max7219->CS,GPIO_PIN_SET);
}