#ifndef DRV_UART_H
#define DRV_UART_H

#ifdef __cplusplus
extern "C" {
#endif

#include "driver/uart.h"

void uart_send_byte(uart_port_t uart_num, uint8_t byte);
void uart_send_buf(uart_port_t uart_num, const uint8_t* buf, size_t len);
esp_err_t uart_receive(uart_port_t uart_num, uint8_t* buf, size_t len, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif

#endif /* DRV_UART_H */