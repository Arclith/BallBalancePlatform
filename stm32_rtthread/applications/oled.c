#include "stm32f1xx_hal.h"
#include "oled_data.h"
#include "stm32f103xb.h"
#include "stm32f1xx_hal_gpio.h"
#include "stm32f1xx_hal_i2c.h"
#include <rtthread.h>
#include "string.h"
#include "ring_buff.h"
#include "oled.h"
#include "shared_drive_init.h"
#include "mylib.h"

#define DBG_TAG "oled"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

void oled_send_command(uint8_t command);
void oled_clear(void);

const uint8_t oled_frame[3][1025];   //帧数据


void oled_init(){

    i2c_init(&my_hi2c1);

    //初始化显示设置
    oled_send_command(0xAE); // 关显示
    oled_send_command(0x20); oled_send_command(0x00); // 水平寻址
    oled_send_command(0xB0); // 起始页
    oled_send_command(0xC8); // 扫描方向
    oled_send_command(0x00); oled_send_command(0x10); // 列地址
    oled_send_command(0x40); // 起始行
    oled_send_command(0x81); oled_send_command(0x7F); // 对比度
    oled_send_command(0xA1); // 段重映射
    oled_send_command(0xA6); // 正常显示
    oled_send_command(0xA8); oled_send_command(0x3F); // 1/64占空比
    oled_send_command(0x8D); oled_send_command(0x14); // 打开充电泵
    oled_send_command(0xAF); // 开显示
    oled_clear();

}

void oled_send_command(uint8_t command){
    frame_head_t frame_head = {
        .data_len = 2,
        .mode = IT,
        .device = oled,
        .magic = 6,
    };
    while(rb_write(&rb_i2c1,&frame_head,(uint8_t[]){ 0x00, command },my_hi2c1.i2c_sem) == -1){
        LOG_D(" rb command full");
    };
}

void oled_set_contrast(uint8_t contrast){
    oled_send_command(0x81);
    oled_send_command(contrast);
}

void oled_set_addr_mode(oled_addr_mode_t mode){
    oled_send_command(0x20);
    oled_send_command(mode);
}

void oled_set_inverse(uint8_t on){
    if(on) oled_send_command(0xA7);
    else oled_send_command(0xA6);
}

void oled_write_str( const uint8_t *str){
    uint8_t len = strlen((const char*)str);
    /* 限制字符串长度，避免 data 数组溢出（最多发送 20 字符） */
    if (len > 20) {
        LOG_W("oled_write_str: input len=%d exceeds 20, truncating", len);
        len = 20;
    }
    uint8_t data[121]; // 最大发送 20 个字符 (6*20+1=121)
    frame_head_t frame_head = {
        .data_len = 6*len+1,
        .mode = DMA,
        .device = oled,
        .magic = 6,
    };
    data[0]=0x40;
    for(int8_t i=0;i<len;i++){
        for(int8_t j=0;j<6;j++){
            data[6*i+j+1]=OLED_F6x8[str[i]-' '][j];
        }
    }
    while(rb_write(&rb_i2c1,&frame_head,data,my_hi2c1.i2c_sem) == -1){
        //rt_kprintf(" rb data full\n");
    };
}

void oled_set_cursor(uint8_t row,uint8_t col){
    uint8_t col_location=col*6-6;
    uint8_t row_location=row-1+0xB0;
    oled_send_command((col_location>>4)+0x10);
    oled_send_command(col_location%16);
    oled_send_command(row_location);
}

void oled_write_str_at(uint8_t row,uint8_t col,const uint8_t *str){
    oled_set_cursor(row,col);
    oled_write_str(str);
}

void oled_clear(void){
    frame_head_t frame_head = {
        .data_len = 129,
        .mode = DMA,
        .device = oled,
        .magic = 6,
    };
    uint8_t data[129] = {0}; 
    data[0] = 0x40;
    for(int i = 1; i < 9; i++){
        oled_set_cursor(i, 1);
        while(rb_write(&rb_i2c1,&frame_head,data,my_hi2c1.i2c_sem) == -1){
            LOG_D(" rb clear full");
        }
    }     
}

void oled_clear_at(uint8_t row,uint8_t col,uint8_t len){
    frame_head_t frame_head = {
        .data_len = 6*len+1,
        .mode = IT,
        .device = oled,
        .magic = 6,
    };
    uint8_t data[6*len+1];
    data[0] = 0x40;
    rt_memset(&data[1],0,6*len);
    oled_set_cursor(row,col);
    while(rb_write(&rb_i2c1,&frame_head,data,my_hi2c1.i2c_sem) == -1){
        LOG_D(" rb clear_at full");
    }     
}

void oled_write_int( int num, uint8_t width) {
    char buf[16];
    char fmt[10];
    rt_sprintf(fmt, "%%-%dd", width); 
    rt_sprintf(buf, fmt, num);
    oled_write_str( (uint8_t *)buf);
}

void oled_write_int_at(uint8_t row, uint8_t col, int num, uint8_t width) {
    char buf[16];
    char fmt[10];
    rt_sprintf(fmt, "%%-%dd", width); 
    rt_sprintf(buf, fmt, num);
    oled_write_str_at(row, col, (uint8_t *)buf);
}

void oled_write_float( float val, uint8_t width) {
    char buf[16];
    ftoa(val,buf,width);
    oled_write_str( (uint8_t *)buf);
}

void oled_write_float_at(uint8_t row, uint8_t col, float val, uint8_t width) {
    char buf[16];
    ftoa(val,buf,width + 1);
    oled_write_str_at(row, col, (uint8_t *)buf);
}

void oled_clear_line(uint8_t row){
    oled_clear_at(row,1,21);
}

void oled_clear_word(uint8_t row, uint8_t col){
        oled_clear_at(row,col,1);
}

void oled_write_tri(uint8_t row,uint8_t col){
    oled_write_str_at(row,col,(uint8_t *)"\x7F");
}

void oled_update_screen(uint8_t key){
    frame_head_t frame_head = {
        .data_len = 1025,
        .mode = DMA,
        .device = oled,
        .magic = 6,
    };
    oled_set_cursor(1,1);
    while(rb_write(&rb_i2c1,&frame_head,oled_frame[key],my_hi2c1.i2c_sem) == -1){
        LOG_D(" rb update full");
    }     
}

void oled_inverse_line(uint8_t key,uint8_t row){
    frame_head_t frame_head = {
        .data_len = 129,
        .mode = DMA,
        .device = oled,
        .magic = 6,
    };
    uint8_t data[129];
    data[0]=0x40;
    for(int8_t i=0;i<128;i++){
        data[i+1]=~oled_frame[key][(row-1)*128+1+i];
    }
    oled_set_cursor(row,1);
    while(rb_write(&rb_i2c1,&frame_head,data,my_hi2c1.i2c_sem) == -1){
        LOG_D(" rb inverse full");
    }
}