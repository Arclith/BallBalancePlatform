#include "ring_buff.h"
#include "rtthread.h"

#define DBG_TAG "rb"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>


void rb_init(rb_t *rb, uint8_t *buffer, uint16_t max_len, rb_hook_t handler)
{
    rb->buffer = buffer;
    rb->max_len = max_len;
    rb->write = 0;
    rb->read = 0;
    rb->data_handler = handler;
    rb->isfree = 1;
    rb->lock = rt_mutex_create("rb_lock", RT_IPC_FLAG_PRIO);
}

uint16_t rb_available_space(rb_t *rb)
{
    uint16_t w = rb->write; 
    uint16_t r = rb->read;  
    if (w >= r) return rb->max_len - (w - r) - 1;
    else return r - w - 1;
}

int8_t rb_write(rb_t *rb, frame_head_t *frame_head, uint8_t *data,rt_sem_t sem)
{
    if(rb->lock) rt_mutex_take(rb->lock, RT_WAITING_FOREVER);

    uint16_t total_len = sizeof(frame_head_t) + frame_head->data_len;
    if (frame_head->data_len > rb->max_len) {
        LOG_E("rb_write: frame_head->data_len(%d) > rb->max_len(%d)", frame_head->data_len, rb->max_len);
        if(rb->lock) rt_mutex_release(rb->lock);
        return -1;
    }
    if (rb->write >= rb->max_len) {
        LOG_E("rb_write: rb->write(%d) >= rb->max_len(%d)", rb->write, rb->max_len);
        if(rb->lock) rt_mutex_release(rb->lock);
        return -1;
    }

    if (rb_available_space(rb) < total_len){
        if(rb->lock) rt_mutex_release(rb->lock);
        return -1;
    }else{
        //数据帧未跨越缓冲区尾部
        if(total_len <= rb->max_len - rb->write){
            rt_memcpy((void*)(rb->buffer + rb->write), (void*)frame_head, sizeof(frame_head_t));
            rt_memcpy((void*)(rb->buffer + rb->write + sizeof(frame_head_t)),(void*)data,frame_head->data_len);
        //数据帧头跨越缓冲区尾部
        }else if(sizeof(frame_head_t) > rb->max_len - rb->write){
            rt_memcpy((void*)(rb->buffer + rb->write), (void*)frame_head, rb->max_len - rb->write);
            rt_memcpy((void*)(rb->buffer), (void*)((uint8_t*)frame_head + rb->max_len - rb->write), sizeof(frame_head_t) - rb->max_len + rb->write);
            rt_memcpy((void*)(rb->buffer  + sizeof(frame_head_t) - (rb->max_len - rb->write)),(void*)data,frame_head->data_len);
        //数据帧数据跨越缓冲区尾部
        }else{
            rt_memcpy((void*)(rb->buffer + rb->write), (void*)frame_head, sizeof(frame_head_t));
            rt_memcpy((void*)(rb->buffer + rb->write + sizeof(frame_head_t)),(void*)data,rb->max_len - rb->write - sizeof(frame_head_t));
            rt_memcpy((void*)(rb->buffer),(void*)(data + rb->max_len - rb->write - sizeof(frame_head_t)),frame_head->data_len -(rb->max_len - rb->write - sizeof(frame_head_t)));
        }       
        rb->write = (rb->write + sizeof(frame_head_t) + frame_head->data_len) % rb->max_len;
    }
    if(rb->isfree){
        if(sem != NULL) rt_sem_take(sem, RT_WAITING_FOREVER);
        rb->data_handler(rb);
    }
    if(rb->lock) rt_mutex_release(rb->lock);
    return 0;
}

//从缓冲区读取数据到data
int8_t rb_read(rb_t *rb,uint8_t *data,uint16_t data_len,frame_head_t *frame_head){
    
    //数据帧头未跨越缓冲区尾部
    if(rb->read + sizeof(frame_head_t) <= rb->max_len){
        rt_memcpy((void*)frame_head,(void*)(rb->buffer + rb->read),sizeof(frame_head_t));
        if(frame_head->data_len > data_len || frame_head->data_len > rb->max_len){
            return -1; //提供的临时读缓冲区不足以存放数据帧或数据长度异常
        }
        //数据未跨越缓冲区尾部
        if(rb->read + sizeof(frame_head_t) + frame_head->data_len <= rb->max_len){
            rt_memcpy((void*)data,(void*)(rb->buffer + rb->read + sizeof(frame_head_t)),frame_head->data_len);
        //数据跨越缓冲区尾部
        }else{
            rt_memcpy((void*)data,(void*)(rb->buffer + rb->read + sizeof(frame_head_t)),rb->max_len - rb->read - sizeof(frame_head_t));
            rt_memcpy((void*)(data + rb->max_len - rb->read - sizeof(frame_head_t)),(void*)(rb->buffer),frame_head->data_len - (rb->max_len - rb->read - sizeof(frame_head_t)));
        }
    //数据帧头跨越缓冲区尾部
    }else{
        rt_memcpy((void*)frame_head,(void*)(rb->buffer + rb->read),rb->max_len - rb->read);
        rt_memcpy((void*)((uint8_t*)frame_head + rb->max_len - rb->read),(void*)(rb->buffer),sizeof(frame_head_t) - (rb->max_len - rb->read));
        if(frame_head->data_len > data_len || frame_head->data_len > rb->max_len){
            return -2; //提供的临时读缓冲区不足以存放数据帧或数据长度异常
        }
        rt_memcpy((void*)data,(void*)(rb->buffer + sizeof(frame_head_t) - (rb->max_len - rb->read)),frame_head->data_len);
    }
    rb->read = (rb->read + sizeof(frame_head_t) + frame_head->data_len) % rb->max_len;
    return 1;
}


//零拷贝方式,data接收数据帧起始地址,返回数据帧长度
//数组尾部数据帧会出现跨越缓冲区尾部的情况,需额外在数据处理函数里面修复
uint16_t rb_peek_linear(rb_t *rb,uint8_t **data){
    frame_head_t frame_head = {0};
    rt_memcpy((void*)&frame_head, (void*)(rb->buffer + rb->read), sizeof(frame_head_t));
    *data = rb->buffer + rb->read +sizeof(frame_head_t);
    return frame_head.data_len;
}

//数据处理回调函数模版
__weak void rb_data_handler(rb_t *rb){
    if(rb->read == rb->write){
        rb->isfree=1;
        return;
    }else{
        rb->isfree=0;
        uint8_t temp_buf[129];
        frame_head_t frame_head;
        if(rb_read(rb,temp_buf,0,&frame_head) == 0){
            LOG_E(" rb read error");
            return;
        }
        /*用户填写对temp_buf的处理*/

    }
}

void rb_ITcallback(rb_t *rb){

    rb->data_handler(rb);

}

void rb_rx_init(rb_rx_t *rb, uint8_t *buffer, uint16_t max_len)
{
    rb->buffer = buffer;
    rb->max_len = max_len;
    rb->write = 0;
    rb->read = 0;
}

int8_t rb_rx_write(rb_rx_t *rb,uint8_t *buf){
    if(rb->write == rb->read - 1) return RT_ERROR;
    rb->buffer[rb->write] = *buf;
    rb->write = (rb->write + 1) % rb->max_len;
    return RT_EOK;
}

int8_t rb_rx_read(rb_rx_t *rb,uint8_t *buf){
    if(rb->read == rb->write) return RT_ERROR;
    *buf = rb->buffer[rb->read];
    rb->read = (rb->read + 1) % rb->max_len;
    return RT_EOK;
}

void rb_rx_clear(rb_rx_t *rb){
    rb->write = 0;
    rb->read = 0;
}
