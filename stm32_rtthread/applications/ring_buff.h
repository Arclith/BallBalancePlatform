#ifndef RING_BUFF_H
#define RING_BUFF_H

#include "stm32f1xx_hal.h"  // 必要 HAL 头
#include <stdint.h>
#include "rtthread.h"

#ifdef __cplusplus
extern "C" {
#endif

// 模块可选的类型定义、结构体、宏等

typedef volatile struct rb_s rb_t;
typedef void (*rb_hook_t)(rb_t *rb);
typedef volatile struct rb_r rb_rx_t;

typedef enum {
    IT = 0,
    DMA = 1
} frame_mode_t;

//环形缓冲区数据帧帧头,包含对该数据帧的描述信息
typedef struct frame_head{
    uint16_t data_len:11;  // 当前数据帧的大小
    frame_mode_t mode:1;      //传输模式,0-中断模式,1-DMA模式
    uint8_t magic:3;
    uint8_t device:1;
}  __attribute__((packed))frame_head_t;

 struct rb_s {
    uint8_t *buffer;      // 环形缓冲区的存储空间
    uint16_t write;        // 写指针
    uint16_t read;        // 读指针
    uint16_t max_len;     // 缓冲区最大长度
    uint8_t isfree;    //当前数据帧是否处理完成
    rb_hook_t data_handler; // 数据自动处理函数
    rt_mutex_t lock;      // 互斥锁，用于保护多线程并发写入
};

 struct rb_r {
    uint8_t *buffer;      // 环形缓冲区的存储空间
    uint16_t write;        // 写指针
    uint16_t read;        // 读指针
    uint16_t max_len;     // 缓冲区最大长度
};

// 消费掉已读取的数据帧,配合rb_peek_linear使用
#define rb_consume(rb, len) \
    do { \
        (rb)->read = (uint16_t)(((rb)->read + (uint16_t)sizeof(frame_head_t) + (uint16_t)(len)) % (rb)->max_len); \
    } while (0)
   
void rb_init(rb_t *rb, uint8_t *buffer, uint16_t max_len,rb_hook_t handler);//静态初始化
int8_t rb_write(rb_t *rb, frame_head_t *frame_head,uint8_t *data,rt_sem_t sem);
int8_t rb_read(rb_t *rb,uint8_t *data,uint16_t data_len,frame_head_t *frame_head);
void rb_rx_init(rb_rx_t *rb, uint8_t *buf, uint16_t max_len);
int8_t rb_rx_write(rb_rx_t *rb,uint8_t *buf);
int8_t rb_rx_read(rb_rx_t *rb,uint8_t *buf);
void rb_rx_clear(rb_rx_t *rb);
void rb_ITcallback(rb_t *rb);//中断回调处理函数,直接放在对应的hal中断回调服务函数中

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFF_H */