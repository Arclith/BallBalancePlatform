#include "driver/uart.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"

void uart_send_byte(uart_port_t uart_num, uint8_t byte)
{
	uart_write_bytes(uart_num, (const char *)&byte, 1);
}

void uart_send_buf(uart_port_t uart_num, const uint8_t* buf, size_t len)
{
	uart_write_bytes(uart_num, (const char *)buf, len);
}

esp_err_t uart_receive(uart_port_t uart_num, uint8_t* buf, size_t len, TickType_t ticks_to_wait)
{
	int read_bytes = uart_read_bytes(uart_num, buf, len, ticks_to_wait);
	if (read_bytes < 0) {
		return ESP_FAIL;
	} else if (read_bytes < len) {
		return ESP_ERR_TIMEOUT;
	} else {
		return ESP_OK;
	}
}

