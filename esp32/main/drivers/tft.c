#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_io_spi.h"  // 自动包含: esp_lcd_types.h
#include "esp_lcd_panel_io.h"  // 需要单独包含: esp_lcd_panel_io_del声明
#include "esp_lcd_panel_ops.h"  // 包含: esp_lcd_panel_draw_bitmap声明
#include "esp_lcd_panel_st7789.h"  // 包含: esp_lcd_panel_dev.h -> esp_lcd_types.h
#include "assert.h"



static const char *TAG = "tft";
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

// CS is on SPI2 bus, other pins must be set to match your board
#define SPI2_TFT_CS_PIN  10
#define SPI2_TFT_DC_PIN  9   // D/C pin, change to your wiring
#define SPI2_TFT_RST_PIN 8   // Reset pin, set to -1 if not used
#define SPI2_TFT_CLK_MHZ	30


static esp_lcd_panel_io_handle_t s_io = NULL;
static esp_lcd_panel_handle_t s_panel = NULL;

esp_err_t tft_drv_install(void)
{
	esp_err_t ret;

	esp_lcd_panel_io_spi_config_t io_cfg = {
		.dc_gpio_num = SPI2_TFT_DC_PIN,
		.cs_gpio_num = SPI2_TFT_CS_PIN,
		.pclk_hz = SPI2_TFT_CLK_MHZ * 1000 * 1000,
		.spi_mode = 0,
		.trans_queue_depth = 2,  
		.lcd_cmd_bits = 8,        
		.lcd_param_bits = 8,      
	};

	ret = esp_lcd_new_panel_io_spi(SPI2_HOST, &io_cfg, &s_io);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "esp_lcd_new_panel_io_spi failed: %d", ret);
		return ret;
	}

	esp_lcd_panel_dev_config_t panel_cfg = {
		.reset_gpio_num = SPI2_TFT_RST_PIN,
		.rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB, // try BGR to match module wiring
		.data_endian = LCD_RGB_DATA_ENDIAN_BIG, // try little-endian data ordering
		.bits_per_pixel = 16,
	};

	ret = esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "esp_lcd_new_panel_st7789 failed: %d", ret);
		esp_lcd_panel_io_del(s_io);
		return ret;
	}
	return ESP_OK;
}

esp_err_t tft_send_cmd(int cmd, const void *param, size_t param_size)
{
	if (s_io == NULL) return ESP_ERR_INVALID_STATE;
	return esp_lcd_panel_io_tx_param(s_io, cmd, param, param_size);
}


static SemaphoreHandle_t sem = NULL;

bool color_trans_done_callback(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx) {
    BaseType_t high_priority_task_woken = pdFALSE;
    xSemaphoreGiveFromISR(sem, &high_priority_task_woken);
    return high_priority_task_woken == pdTRUE;
}

esp_err_t tft_init(void){

	if(tft_drv_install() != ESP_OK){
		return ESP_FAIL;
	}vTaskDelay(20);

	if (s_panel == NULL) {
		ESP_LOGE(TAG, "TFT panel not installed");
		return ESP_ERR_INVALID_STATE;
	}vTaskDelay(20);

	esp_lcd_panel_reset(s_panel);vTaskDelay(100);
		
	esp_err_t ret = esp_lcd_panel_init(s_panel);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "esp_lcd_panel_init failed: %d", ret);
		return ret;
	}vTaskDelay(20);
	ESP_LOGI(TAG, "panel initialized");

	ret = esp_lcd_panel_swap_xy(s_panel, true);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "esp_lcd_panel_swap_xy failed: %d", ret);
		return ret;
	}vTaskDelay(20);
	ESP_LOGI(TAG, "panel XY swapped for 320x240 resolution");

	esp_lcd_panel_mirror(s_panel, false, true);vTaskDelay(20);

	esp_lcd_panel_invert_color(s_panel, true);vTaskDelay(20);

	ret = esp_lcd_panel_disp_on_off(s_panel, true);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "esp_lcd_panel_disp_on_off failed: %d", ret);
		return ret;
	}vTaskDelay(20);
	ESP_LOGI(TAG, "panel display turned on (backlight must be enabled separately if any)");

	sem = xSemaphoreCreateBinary();
	if (sem == NULL) {
		ESP_LOGE(TAG, "Failed to create semaphore");
		return ESP_FAIL;
	}

	esp_lcd_panel_io_callbacks_t cbs = {
    	.on_color_trans_done = color_trans_done_callback,
	};
	esp_lcd_panel_io_register_event_callbacks(s_io, &cbs, NULL);

	return ESP_OK;
}

esp_err_t tft_display(int x0, int y0, int x1, int y1, const void *bitmap,uint32_t timeout_ms)
{
	if (s_panel == NULL) return ESP_ERR_INVALID_STATE;
	esp_err_t ret = esp_lcd_panel_draw_bitmap(s_panel, x0, y0, x1, y1, bitmap);
	if (ret != ESP_OK) {
		return ret;
	}
	if (xSemaphoreTake(sem, timeout_ms) != pdTRUE) {
		ESP_LOGE(TAG, "Timeout waiting for color transfer to complete");
		return ESP_ERR_TIMEOUT;
	}
	return ESP_OK;
}

void tft_clear(void){
	uint8_t *buf = (uint8_t *)heap_caps_aligned_alloc(16, 240*320*2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(buf != NULL && "Failed to allocate memory when clearing");
    if(buf) ESP_LOGI(TAG, "Memory allocated");
	memset(buf, 0x00, 240*320*2); // 填充黑色
	tft_display(0, 0, 320, 240, buf, 100);
    free(buf);
}