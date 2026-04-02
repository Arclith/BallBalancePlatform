#include "rtthread.h"
#include "stm32f1xx_hal.h"
#include "shared_drive_init.h"
#include "drv_uart.h"
#include "mylib.h"

#define DBG_TAG "comm_thread"
#define DBG_LVL DBG_INFO
#include "rtdbg.h"


#define ACK_RETRY_MS    5 //ack等待重发时间

static rt_thread_t communicate_thread = RT_NULL;
static rt_mailbox_t mb_to_servo = RT_NULL;
static uint8_t tx_seq;
static uint8_t rx_seq;

typedef struct {
    uint8_t mail_id;
    float f;
}fmail_t;

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

void send_ack(uint8_t recv_seq){
    uart_send_byte(&my_huart3, recv_seq);
}

uint8_t wait_ack(uint16_t timeout_ms){
    uint8_t buf;
    if(uart_receive(&my_huart3, &buf, 1, timeout_ms) == RT_EOK){
        if(buf == tx_seq){
            tx_seq++;
            return RT_EOK;
        }else{
            return RT_ERROR;
        }
    }else{
        LOG_D("wait_ack timeout");
        return RT_ETIMEOUT;
    }
}

//格式: 0xAA 0xBB | seq | len | data... | crc8
void send_to_esp(const uint8_t* data_buf, uint8_t len,uint16_t timeout_ms){
    if(len > 27){
        LOG_E("send_buf not enough");
        return;
    }
    uint8_t send_buf[32] = {0xAA,0XBB};
    uint8_t i = 2;
    send_buf[i++] = tx_seq;
    send_buf[i++] = len;
    uint8_t rtery_count = timeout_ms/ACK_RETRY_MS;
    rt_memcpy(&send_buf[i], data_buf, len);
    i += len;
    send_buf[i] = crc8(send_buf + 2, len + 2);
    i = 0;
    do{
        uart_send_buf(&my_huart3, send_buf, len + 5);
        i++;
        if(i > rtery_count){
            LOG_E("send_to_esp timeout");
            return;
        }
    }while(wait_ack(ACK_RETRY_MS) != RT_EOK);
}
 
uint8_t listen_to_esp(uint8_t* data_buf, uint16_t len){
    uint8_t recv_buf[32];
    uint8_t i = 0;
    LOG_D("listening to esp...");
    uart_receive( &my_huart3, recv_buf + i, 1, RT_WAITING_FOREVER);
    if(recv_buf[i++] != 0xAA){
        return RT_ERROR;
    }
    uart_receive( &my_huart3, recv_buf + i, 1, RT_WAITING_FOREVER);
    if(recv_buf[i++] != 0xBB){
        return RT_ERROR;
    }
    LOG_D("frame head ok");
     uart_receive( &my_huart3, &recv_buf[i], 2, RT_WAITING_FOREVER);
    if(recv_buf[i++] == rx_seq) {//seq相等代表是重复数据,跳过
        uart_rx_buf_clear(&my_huart3);
        send_ack(rx_seq);
        LOG_D("data repeat,seq:%d",rx_seq);
        return RT_ERROR;
    }
    rx_seq = recv_buf[i-1];
    uint8_t data_len = recv_buf[i++];
    if(data_len > len){
        LOG_E("data len too long,ect:%d,buf:%d",data_len,len);
        return RT_ERROR;
    }
    uart_receive(&my_huart3, &recv_buf[i], data_len, RT_WAITING_FOREVER);
    i += data_len;
    LOG_D("frame tail ok");
    uart_receive(&my_huart3, &recv_buf[i], 1, RT_WAITING_FOREVER);
    uint8_t crc = crc8(&recv_buf[2], data_len + 2);
    if(recv_buf[i] != crc){
        LOG_E("crc error,ect:%02X,calc:%02X",recv_buf[i],crc);
        return RT_ERROR;
    }
    LOG_D("crc ok");
    send_ack(rx_seq);
    rt_memcpy(data_buf, &recv_buf[4], data_len);
    return RT_EOK;
}

float cam_fps = 0,lcd_fps = 0,fx = 0,fy = 0;
float rx_fps = 0; // 接收帧率变量

void calc_rx_fps(){
    static uint32_t last_time = 0;
    static uint32_t count = 0;
    count++;
    uint32_t now = rt_tick_get();
    if(now - last_time >= 1000){
        rx_fps = (float)count * 1000.0f / (float)(now - last_time);
        char buf[4];
        ftoa(rx_fps, buf, 4); // 将帧率转换为字符串，保留两位小数
        count = 0;
        last_time = now;
        LOG_I("RX FPS: %s", buf);
    }
}

rt_mq_t mq_xy = NULL;
void communicate_thread_entry(void *parameter){
    uart_init(&my_huart3);
    mb_to_servo = rt_mb_create("mb_to_servo", 5, RT_IPC_FLAG_FIFO);
    mq_xy = rt_mq_create("mq_xy", 8, 2, RT_IPC_FLAG_FIFO);
    while(1){

#ifdef IDLE_CNT_TEST_ENABLE
        rt_thread_delay(10000);
#endif  

        uint8_t recv_buf[10] = {0};
        if(listen_to_esp(recv_buf, 32) == RT_EOK){
            if(recv_buf[0] == 1){
                cam_fps = *((float*)&recv_buf[1]);
            }else if(recv_buf[0] == 2){
                rt_mq_send(mq_xy,  recv_buf + 1, 8);
                fx = *((float*)&recv_buf[1]);
                fy = *((float*)&recv_buf[5]);
               // calc_rx_fps(); // 计算接收帧率
            }else if(recv_buf[0] == 3){
                lcd_fps = *((float*)&recv_buf[1]);
            }

        }
    }

}

int communicate_thread_init(){
    communicate_thread = rt_thread_create("comm_thread",
                                   communicate_thread_entry,
                                   RT_NULL,
                                   512,
                                   6,
                                   10);

    rt_thread_startup(communicate_thread);
    return 0;
}
INIT_APP_EXPORT(communicate_thread_init);