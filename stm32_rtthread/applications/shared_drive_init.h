#ifndef PERIPHERAL_INIT_H
#define PERIPHERAL_INIT_H

#include "stm32f1xx_hal.h"  // 必要 HAL 头
#include <stdint.h>
#include "ring_buff.h"
#include "rtthread.h"

#ifdef __cplusplus
extern "C" {
#endif



//基类
typedef struct {
    uint8_t count:4;// 引用计数
    uint8_t port:2;//外设号
    uint8_t IT:1; //是否使用中断
    uint8_t DMA:1;//是否使用DMA
    rb_t rb;//环形缓冲区
    rt_sem_t rx_sem;//用于接收同步的信号量
} drive_base_t;


//i2c
#define RB_I2C1_SIZE 1500
#define I2C1_READ_TEMP_SIZE 1025

typedef struct my_hi2c {
    drive_base_t base;
    I2C_HandleTypeDef hi2c;
    rt_sem_t i2c_sem;
    DMA_HandleTypeDef hdma_tx;
    DMA_HandleTypeDef hdma_rx;
}my_hi2c_handle_t;

 enum {
    oled = 0,
    mpu = 1
};

extern my_hi2c_handle_t my_hi2c1;
extern rb_t rb_i2c1;
extern uint8_t i2c1_buf[RB_I2C1_SIZE];
extern rt_mutex_t i2c1_mutex;

void i2c_init(my_hi2c_handle_t *hi2c);


//spi
typedef struct my_hspi {
    drive_base_t base;
    SPI_HandleTypeDef hspi;
    DMA_HandleTypeDef hdma_tx;
    DMA_HandleTypeDef hdma_rx;
}my_hspi_handle_t;

#define RB_SPI2_SIZE 500
#define SPI2_READ_TEMP_SIZE 129

extern my_hspi_handle_t my_hspi2;
extern rb_t rb_spi2;    
extern uint8_t spi2_buf[RB_SPI2_SIZE];

void spi_init(my_hspi_handle_t *hspi);

enum{
    w25q = 0,

};

//uart1

typedef struct my_huart {
    drive_base_t base;
    UART_HandleTypeDef huart;
    DMA_HandleTypeDef hdma_tx;
    DMA_HandleTypeDef hdma_rx;
    rb_rx_t rb_rx;
}my_huart_handle_t;

#define RB_UART1_SIZE 128 
#define UART1_READ_TEMP_SIZE 20
extern my_huart_handle_t my_huart1;
extern uint8_t uart1_buf[RB_UART1_SIZE];
      
#define RB_UART3_SIZE 256
#define UART3_READ_TEMP_SIZE 20
extern my_huart_handle_t my_huart3;
extern uint8_t uart3_buf[RB_UART3_SIZE];

#define RB_UART3_RX_SIZE 256
extern uint8_t uart3_rx_buf[RB_UART3_RX_SIZE];



void uart_init(my_huart_handle_t *my_huart);

#ifdef __cplusplus
}
#endif

#endif /* PERIPHERAL_INIT_H */