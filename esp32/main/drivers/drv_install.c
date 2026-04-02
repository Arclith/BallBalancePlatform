#include "driver/uart.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "drv_install";


#define UART_TX_BUF_SIZE (1024)
#define UART_RX_BUF_SIZE (1024)

#define SPI2_SDA_PIN 11
#define SPI2_SCL_PIN 12

static QueueHandle_t s_uart_queue = NULL;

esp_err_t uart0_install();
esp_err_t spi2_install();

esp_err_t drv_install(void){
	esp_err_t ret = ESP_OK;

	ret = uart0_install();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "uart0_install failed: %d", ret);
		return ret;
	}

	ret = spi2_install();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "spi2_install failed: %d", ret);
		return ret;
	}

	ESP_LOGI(TAG, "All drivers installed successfully");
	return ESP_OK;
}

esp_err_t uart0_install(){
	esp_err_t ret;

	uart_config_t uart_cfg = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.rx_flow_ctrl_thresh = 1,
	};

	ret = uart_param_config(0, &uart_cfg);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "uart_param_config failed: %d", ret);
		return ret;
	}

	ret = uart_set_pin(0, 43, 44, -1, -1);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "uart_set_pin failed: %d", ret);
		return ret;
	}


	ret = uart_driver_install(0, UART_RX_BUF_SIZE, UART_TX_BUF_SIZE, 10, &s_uart_queue, 0);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "uart_driver_install failed: %d", ret);
		return ret;
	}

	return ESP_OK;
}


esp_err_t spi2_install(){

	spi_bus_config_t buscfg = {
		.miso_io_num = -1,
		.mosi_io_num = SPI2_SDA_PIN,
		.sclk_io_num = SPI2_SCL_PIN,
		.quadwp_io_num = -1,
		.quadhd_io_num = -1,
		.max_transfer_sz = 240*320*2,
	};

	esp_err_t ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "spi_bus_initialize failed: %d", ret);
		return ret;
	}
	ESP_LOGI(TAG, "spi_bus_initialize succeeded");
	return ESP_OK;
}
