//8个引脚接在同一端口,行作为输出,列作为输入
#include "keypad.h"
#include "stm32_hal_legacy.h"
#include "stm32f103xb.h"
#include "stm32f1xx_hal.h"
#include "stm32f1xx_hal_gpio.h"
#include <stdint.h>



//按键状态枚举,即状态机转换表索引
typedef enum{
    idle,
    process,
    press
}key_state_t;

//表示一个按键的结构体,存放当前按键状态,记录时间
typedef struct{
    key_state_t key_state;
    uint32_t now_tick;
    uint8_t key_isDown; 
    uint8_t key_location;
}key_struct_t;

//全局存放按键状态结构体的数组
key_struct_t key[16]={0};

// 回调函数数组定义
key_callback_t key_callback[16] = {0};

//状态机函数指针
typedef void (*key_state_list_t)(key_struct_t*);

void key_state_idle(key_struct_t *key);
void key_state_press(key_struct_t *key);
void key_state_process(key_struct_t *key);


//状态机转移表
const key_state_list_t key_state_list[3]={key_state_idle,key_state_process,key_state_press};

//初始化表示每个按键的结构体
void keypad_reset(){
    uint8_t i = 0;
    while(i<16){
        key[i].key_state=idle;
        key[i].now_tick=0;
        key[i].key_isDown=0;
        key[i].key_location=i;
        i++;
    }
}

//键盘初始化
void keypad_init(keypad_t *keypad){

    if (keypad->GPIOx == GPIOA) __HAL_RCC_GPIOA_CLK_ENABLE();
    else if (keypad->GPIOx == GPIOB) __HAL_RCC_GPIOB_CLK_ENABLE();
    else if (keypad->GPIOx == GPIOC) __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct1={0},GPIO_InitStruct2={0};
    GPIO_InitStruct1.Mode=GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct1.Pin=keypad->pin_row[0]|keypad->pin_row[1]|keypad->pin_row[2]|keypad->pin_row[3];
    GPIO_InitStruct1.Pull=GPIO_NOPULL;
    GPIO_InitStruct1.Speed=GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct2.Mode=GPIO_MODE_INPUT;
    GPIO_InitStruct2.Pin=keypad->pin_col[0]|keypad->pin_col[1]|keypad->pin_col[2]|keypad->pin_col[3];
    GPIO_InitStruct2.Pull=GPIO_PULLDOWN;
    GPIO_InitStruct2.Speed=GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(keypad->GPIOx, &GPIO_InitStruct1);//行对应端口,推挽输出
    HAL_GPIO_Init(keypad->GPIOx, &GPIO_InitStruct2);//列对应端口,下拉输入

    HAL_GPIO_WritePin(keypad->GPIOx, GPIO_InitStruct1.Pin, GPIO_PIN_RESET);//设置行相连引脚默认为低电平
    keypad_reset();
}

/**
  * @brief  扫描获取矩阵键盘当前状态
  * @param  keypad:矩阵键盘引脚结构体
  * @retval 矩阵键盘当前状态的掩码,位数表示序号,位值表示状态,最低位对应第一个按键
  */
uint16_t keypad_scan(keypad_t *keypad){
    uint16_t mask=0x0000,t=0x0001;
    uint8_t pin_state;
    for(uint8_t i=0;i<4;i++){
        HAL_GPIO_WritePin(keypad->GPIOx, keypad->pin_row[i], GPIO_PIN_SET);
        for(uint8_t j=0;j<4;j++){
            pin_state=HAL_GPIO_ReadPin(keypad->GPIOx, keypad->pin_col[j]);
            if(pin_state==GPIO_PIN_SET){mask|=t;}
            t<<=1;
        }
        HAL_GPIO_WritePin(keypad->GPIOx, keypad->pin_row[i], GPIO_PIN_RESET);
    }
    return mask;
}


//状态机处理函数,同时也为主函数模块接口
void keypad_update(keypad_t *keypad){
    uint16_t mask = keypad_scan(keypad),t = 0x0001;
    for(uint8_t i = 0;i < 16;i++){
        if((mask&t)&&key[i].key_state!=process) 
            key[i].key_isDown=1;
        if((!(mask&t))&&key[i].key_state!=process) key[i].key_isDown=0;
        (*key_state_list[key[i].key_state])(key+i);
        t<<=1;
    }
}

//空闲状态
void key_state_idle(key_struct_t *key){
    if(key->key_isDown){
        key->now_tick=HAL_GetTick();
        key->key_state=process;
    }
}

//消抖中
void key_state_process(key_struct_t *key){
    uint32_t now_tick=HAL_GetTick();
    if(now_tick - key->now_tick > 40){
        if(key->key_isDown) key->key_state=press;
        else key->key_state=idle;
    }
}

//确定按下,松开时触发
void key_state_press(key_struct_t *key){
    if(key->key_isDown==0){
        (*key_callback[key->key_location])(key->key_location);
        key->key_state=idle;
    }
}
