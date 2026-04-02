#ifndef __KEYPAD_H__
#define __KEYPAD_H__

#include "stm32f1xx_hal.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

enum KeyValue {
                          
    KEY_PGDN  = 0,     
    KEY_UP    = 1,         
    KEY_PGUP  = 2,
    KEY_A     = 3,       //    KEY_PGUP      KEY_UP           KEY_PGDN          x
    
    KEY_LEFT  = 4,     
    KEY_5     = 5, 
    KEY_RIGHT = 6, 
    KEY_B     = 7,     //      KEY_LEFT        x            KEY_RIGHT           x
    
    KEY_BACK  = 8,    
    KEY_DOWN  = 9, 
    KEY_OK    = 10, 
    KEY_C     = 11,    //      KEY_BACK     KEY_DOWN          KEY_OK           x
    
    KEY_CFM  = 12,       
    KEY_0     = 13, 
    KEY_HASH  = 14, 
    KEY_D     = 15     //          x            x            x                 x
};


//模块接口结构体
typedef struct{
    uint32_t pin_row[4];//和行连接的对应的4个引脚
    uint32_t pin_col[4];//和列连接的对应的4个引脚
    GPIO_TypeDef *GPIOx;//所在端口
}keypad_t;

//回调函数数组,主函数中根据按键功能自定义
typedef void (*key_callback_t)(uint8_t);
extern key_callback_t key_callback[16];


void keypad_update(keypad_t *keypad);
void keypad_init(keypad_t *keypad);


#ifdef __cplusplus
}
#endif

#endif /* __MODULE_H__ */
