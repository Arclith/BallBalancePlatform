#ifndef __serial_H__
#define __serial_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "shared_drive_init.h"

void uart_send_str(my_huart_handle_t *my_huart, const char* str);
void uart_send_buf(my_huart_handle_t *my_huart,uint8_t* buf, uint16_t len);
void uart_send_float(my_huart_handle_t *my_huart,float f);
void uart_send_byte(my_huart_handle_t *my_huart,uint8_t byte);
void uart_send_int(my_huart_handle_t *my_huart,int data);
uint8_t uart_receive(my_huart_handle_t *my_huart, 
                    uint8_t* data_buf, 
                    uint16_t len,
                    uint16_t timeout_ms);
void uart_rx_buf_clear(my_huart_handle_t *my_huart);

#ifdef __cplusplus
}
#endif

#endif /* __serial_H__ */