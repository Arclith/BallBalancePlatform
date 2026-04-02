#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "drv_uart.h"
#include <string.h>
#include "esp_err.h"
#include "typedef.h"

static const char *TAG = "comm_task";
#define LOG_LOCAL_LEVEL ESP_LOG_INFO
#include "esp_log.h"

#define ACK_RETRY_MS 5

uint8_t seq = 0;
extern QueueHandle_t xMailbox;


uint8_t crc8(const uint8_t *data, uint16_t len)
{
    uint8_t crc = 0xFF;
    for (uint16_t i = 0; i < len; i++) {
        uint8_t b = data[i];   
        crc ^= b;
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc & 0x80) ? ((uint8_t)((crc << 1) ^ 0x31)) : ((uint8_t)(crc << 1));
        }
    }
    return crc;
}

void send_ack(uint8_t seq_recv){
    uart_send_byte(0, seq_recv);
}

uint8_t wait_ack(uint16_t timeout_ms){
    uint8_t buf;
    uint32_t start_tick = xTaskGetTickCount();
    while(xTaskGetTickCount() - start_tick < pdMS_TO_TICKS(timeout_ms)){
        int ret = uart_receive(0, &buf, 1, timeout_ms);
        if(ret == ESP_OK){
            if(buf == seq){
                ESP_LOGD(TAG,"ack ok");
                seq++;
                return ESP_OK;
            }else{
                ESP_LOGD(TAG,"ack error");
            }
        }else if(ret == ESP_ERR_TIMEOUT){
            ESP_LOGD(TAG,"ack timeout");
        }else if(ret == ESP_FAIL){
            ESP_LOGD(TAG,"ack  error");
        }
    }
    return ESP_ERR_TIMEOUT;
}

void send_to_stm(const uint8_t* data_buf, uint8_t len,uint16_t timeout_ms){
    if(len > 27){
        ESP_LOGE(TAG,"send_buf not enough");
        return;
    }
    uint8_t send_buf[32] = {0xAA,0XBB};
    uint8_t i = 2;
    send_buf[i++] = seq;
    send_buf[i++] = len;
    uint8_t rtery_count = timeout_ms/ACK_RETRY_MS;
    memcpy(&send_buf[i], data_buf, len);
    i += len;
    send_buf[i] = crc8(send_buf + 2, len + 2);
    i = 0;
    do{
        uart_send_buf(0, send_buf, len + 5);
        i++;
        if(i > rtery_count){
            ESP_LOGD(TAG,"send_to_stm timeout");
            return;
        }
    }while(wait_ack(ACK_RETRY_MS) != ESP_OK);
}
 
uint8_t listen_to_stm(uint8_t* data_buf, uint16_t len){
    uint8_t recv_buf[32];
    uint8_t i = 0;
    ESP_LOGI(TAG, "listening to stm...");
    uart_receive(0, recv_buf + i, 1, -1);
    if(recv_buf[i++] != 0xAA){
        return ESP_ERR_INVALID_RESPONSE;
    }
    uart_receive(0, recv_buf + i, 1, -1);
    if(recv_buf[i++] != 0xBB){
        return ESP_ERR_INVALID_RESPONSE;
    }
    ESP_LOGD(TAG, "frame head ok");
     uart_receive(0, &recv_buf[i], 2, -1);
    uint8_t seq_recv = recv_buf[i++];
    uint8_t data_len = recv_buf[i++];
    if(data_len > len){
        ESP_LOGE(TAG, "data len too long,ect:%d,buf:%d",data_len,len);
        return ESP_ERR_INVALID_RESPONSE;
    }
    uart_receive(0, &recv_buf[i], data_len, -1);
    i += data_len;
    ESP_LOGD(TAG, "frame tail ok");
    uart_receive(0, &recv_buf[i], 1, -1);
    uint8_t crc = crc8(&recv_buf[2], data_len + 2);
    if(recv_buf[i] != crc){
        ESP_LOGE(TAG, "crc error,ect:%02X,calc:%02X",recv_buf[i],crc);
        return ESP_ERR_INVALID_CRC;
    }
    ESP_LOGD(TAG, "crc ok");
    send_ack(seq_recv);
    memcpy(data_buf, &recv_buf[4], data_len);
    return ESP_OK;
}


void communicate_task_entry(void *p){

    while(1){
        uint8_t f[10];
        if(xQueueReceive(xMailbox, f, portMAX_DELAY) == pdTRUE){
            if(f[0] == 1 || f[0] == 3) send_to_stm(f, 5, 1000);
            if(f[0] == 2) send_to_stm(f, 9, 1000);
        }
    }
}

void communicate_task_init(void){
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    xTaskCreate(communicate_task_entry,
                            "comm_task",
                                4096,
                                NULL,
                                20,
                                NULL);
}