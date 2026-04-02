#include "rtthread.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_uart.h"
#include <stdint.h>
#include <string.h>
#include <stm32f1xx_hal_rcc.h>
#include "shared_drive_init.h"
#include "ring_buff.h"

#define DBG_TAG "serial"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>


void uart_send_str(my_huart_handle_t *my_huart, const char* str){
    frame_head_t frame_head = {
        .data_len = (uint16_t)strlen(str),
        .mode = IT,
        .magic = 6,
        .device = 0,
    };
    while(rb_write(&my_huart->base.rb,&frame_head,(uint8_t*)str,NULL) == -1){
        LOG_D(" rb uart1 full");
    };

}

void uart_send_buf(my_huart_handle_t *my_huart,uint8_t* buf, uint16_t len){
    frame_head_t frame_head = {
        .data_len = len,
        .mode = IT,
        .magic = 6,
        .device = 0,
    };
    while(rb_write(&my_huart->base.rb,&frame_head,buf,NULL) == -1){
        LOG_D(" rb uart1 full");
    };

}

void uart_send_float(my_huart_handle_t *my_huart,float f){
    uint8_t buf[4];
    rt_memcpy(buf, &f, 4);
    uart_send_buf(my_huart,buf, 4);
}

void uart_send_byte(my_huart_handle_t *my_huart,uint8_t byte){
    uint8_t buf = byte;
    uart_send_buf(my_huart,&buf,1);
}

void uart_send_int(my_huart_handle_t *my_huart,int num){
    char buf[4];
    rt_memcpy(buf, &num, 4);
    uart_send_buf(my_huart,buf,4);
}

uint8_t uart_receive(my_huart_handle_t *my_huart, 
                    uint8_t* data_buf, 
                    uint16_t len,
                    uint16_t timeout_ms){
    for(uint16_t i = 0; i < len; i++){
        if(rt_sem_take(my_huart->base.rx_sem, timeout_ms) != RT_EOK){
            return i;
        }
        rb_rx_read(&my_huart->rb_rx, &data_buf[i]);
    }
    return RT_EOK;
}

void uart_rx_buf_clear(my_huart_handle_t *my_huart){
    rb_rx_clear(&my_huart->rb_rx);
}

extern uint8_t rx_buf;
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart){
    if(huart->Instance == USART3){
        rb_rx_write(&my_huart3.rb_rx, &rx_buf);
        HAL_UART_Receive_IT(&my_huart3.huart, &rx_buf, 1);
        rt_sem_release(my_huart3.base.rx_sem);
    }
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{

    LOG_E("UART Error: 0x%lx", huart-> ErrorCode);

}
