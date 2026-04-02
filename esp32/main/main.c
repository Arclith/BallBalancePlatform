/*                              
可用gpio
低位组: GPIO 1, 2, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 21
高位组: GPIO 38, 39, 40, 41, 42, 47
 */




#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_camera.h"      
#include "my_ov7670.h"
#include "driver/usb_serial_jtag.h"
#include "driver/gpio.h"
#include "esp_task_wdt.h"
#include "esp_freertos_hooks.h"
#include "typedef.h"
#include "tft.h"

#define CAM_TO_TFT_ENABLE 

static const char *TAG = "MAIN";

#define MAX(a, b) ((a) > (b) ? (a) : (b))
esp_err_t drv_install(void);
void communicate_task_init(void); 
void cam_task_init(void);
void lcd_task_init(void);
void shell_task_init(void);
void yuyv_to_binary(uint16_t *buf, uint16_t *binary, int width, int height, uint8_t thr);
void yuyv_to_binary_self(uint16_t *buf,int width, int height, uint8_t thr) {
    int len = width * height;
    for (int i = 0; i  < len; i++) {
        uint8_t y = buf[i]&0x00FF;
        buf[i] = (y < thr) ? 0 : 0xFFFF;
    }
}

#ifdef CAM_TO_TFT_ENABLE
void app_main(void){
    drv_install();
#ifdef DEBUG
    tft_init();
    cam_task_init();
#else
    cam_task_init();
    lcd_task_init();
#endif
    shell_task_init();
    communicate_task_init();
    gpio_set_level(48,1);
    ESP_LOGE(TAG,"time:12:54");

    while(1){
        vTaskDelay(500000);
    }
}


#else

static QueueHandle_t frame_queue = NULL;
static SemaphoreHandle_t usb_mutex = NULL;
/**
 * @brief 压缩工人：从队列抢图，并行压缩，竞争 USB 发送权
 */
void compression_worker(void *pvParameters)
{
    int core_id = (int)pvParameters;
    while (1) {
        camera_fb_t *fb = NULL;
        if (xQueueReceive(frame_queue, &fb, portMAX_DELAY) == pdTRUE) {
            uint8_t *jpg_buf = NULL;
            size_t jpg_len = 0;
            
            // 软件并行压缩阶段 (利用 S3 的 PIE 向量指令)
            if (core_id == 1) gpio_set_level(21, 0); 
            bool converted = frame2jpg(fb, 30, &jpg_buf, &jpg_len);
            if (core_id == 1) gpio_set_level(21, 1);

            if (converted) {
                if (xSemaphoreTake(usb_mutex, portMAX_DELAY) == pdTRUE) {
                    uint8_t head[] = {0xAA, 0xBB, 0xCC, 0xDD, 0x00, 0x11, 0x22, 0x33};
                    uint8_t tail[] = {0x33, 0x22, 0x11, 0x00, 0xDD, 0xCC, 0xBB, 0xAA};
                    uint32_t len = jpg_len;

                    usb_serial_jtag_write_bytes(head, 8, pdMS_TO_TICKS(10));
                    usb_serial_jtag_write_bytes((uint8_t*)&len, 4, pdMS_TO_TICKS(10));
                    usb_serial_jtag_write_bytes(jpg_buf, jpg_len, pdMS_TO_TICKS(500));
                    usb_serial_jtag_write_bytes(tail, 8, pdMS_TO_TICKS(10));
                    
                    xSemaphoreGive(usb_mutex);
                }
                free(jpg_buf); 
            }
            esp_camera_fb_return(fb);
        }
    }
}


/// @brief 
/// @param  
void app_main(void)
{
    // 配置 USB Serial JTAG 驱动。
    // 使用较大的 TX 缓冲区来平滑大数据量的发送。
    usb_serial_jtag_driver_config_t jtag_config = {
        .tx_buffer_size = 16384, // 16KB 缓存
        .rx_buffer_size = 1024,
    };
    
    // 如果系统已经占用了，先尝试卸载（防止冲突）
    usb_serial_jtag_driver_uninstall(); 
    if (usb_serial_jtag_driver_install(&jtag_config) != ESP_OK) {
        return;
    }

    // 初始化 GPIO 21 用于逻辑分析仪测量
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << 21),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    gpio_set_level(21, 1); // 默认高电平

    // 严禁任何日志输出，防止污染数据流
    esp_log_level_set("*", ESP_LOG_NONE);

    ov7670_init();

    
    vTaskDelay(pdMS_TO_TICKS(500)); 

    ov7670_set_night_mode(false); // 明确关闭
    ov7670_color_test(0); 

    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        ESP_LOGI(TAG, "Safe configuration complete. Starting warm-up...");
        for(int i=0; i<5; i++){
            ESP_LOGI(TAG, "Fetching warm-up frame %d...", i);
            camera_fb_t *tfb = esp_camera_fb_get();
            if(tfb) {
                ESP_LOGI(TAG, "Frame %d OK (len: %zu)", i, tfb->len);
                esp_camera_fb_return(tfb);
            } else {
                ESP_LOGW(TAG, "Frame %d TIMEOUT!", i);
            }
        }
    }

    // 1. 创建同步组件
    frame_queue = xQueueCreate(4, sizeof(camera_fb_t *));
    usb_mutex = xSemaphoreCreateMutex();

    // 2. 启动两个核心的工人（交叉并行）
    xTaskCreatePinnedToCore(compression_worker, "comp0", 8192, (void*)0, 4, NULL, 0);
    xTaskCreatePinnedToCore(compression_worker, "comp1", 8192, (void*)1, 5, NULL, 1);

    ESP_LOGI(TAG, "Reverted to Dual-Core Software PIE. (S3 has no HW Encoder)");
    
     void * p = (uint8_t *)heap_caps_malloc(240*320*2, MALLOC_CAP_SPIRAM);
    while (1) {
        esp_task_wdt_reset(); 
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            // 核心 0 只管拼命喂图到队列里
     //     yuyv_to_binary((uint16_t *)fb->buf, (uint16_t *)p, fb->width, fb->height, 128);
     //       yuyv_to_binary_self((uint16_t *)fb->buf, fb->width, fb->height, 128);
            if (xQueueSend(frame_queue, &fb, 0) != pdTRUE) {
                esp_camera_fb_return(fb);
            }
        }
        // 对于 DMA 采集，这里不需要长延时，1 tick 即可
        vTaskDelay(1); 
    }
}

#endif