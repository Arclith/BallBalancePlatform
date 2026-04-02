#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "tft.h"
#include "driver/gpio.h"
#include "esp_timer.h" 
#include "esp_camera.h"  
#include "esp_async_memcpy.h"

static const char *TAG = "lcd_task";
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

volatile bool Completion = 1;
extern SemaphoreHandle_t sem_cam;
extern volatile uint8_t *binary;
extern  QueueHandle_t xMailbox;

static void cal_frame_rate(){

    static uint32_t frame_count = 0;
    static uint64_t start_time = 0;
    
    if (frame_count == 0) {
        start_time = esp_timer_get_time();
    }
    
    frame_count++;
    
    if (frame_count % 30 == 0) {
        uint64_t current_time = esp_timer_get_time();
        uint64_t elapsed_us = current_time - start_time;
        float fps = (frame_count * 1000000.0f) / elapsed_us;
       // ESP_LOGI(TAG,"tft fps:%f",fps);
       uint8_t buf[5];
        buf[0] = 3;
        memcpy(buf+1,&fps,4);
        xQueueSend(xMailbox, buf, 0); 
                
        // 重置计数器
        frame_count = 0;
        start_time = current_time;
    }
}

extern 
void lcd_task_entry(void *p){
    while(1){
        xSemaphoreTake(sem_cam, portMAX_DELAY);
        tft_display(0,0,248,240,binary,1000);
        Completion = 1;
        cal_frame_rate();
    }
}

void lcd_task_init(void){
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    tft_init();
    tft_clear();
    xTaskCreate(lcd_task_entry,
                "lcd_task",
                4096,
                NULL,
                19,
                NULL);
}