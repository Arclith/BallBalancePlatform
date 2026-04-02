#include "stm32f1xx_hal.h"
#include <rtthread.h>
#include "keypad.h"

#define DBG_TAG "keypad_thread"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

#define MB_TO_OLED_SIZE 4  
#define MB_TO_SERVO_SIZE 4
rt_sem_t keypad_sem = NULL;



static rt_thread_t keypad_thread = RT_NULL;
struct rt_mailbox mb_to_oled;   
struct rt_mailbox mb_to_servo;  

static rt_ubase_t mb_pool1[MB_TO_OLED_SIZE];         // 修正为4个单位大小（每个单位4字节），总共16字节
static rt_ubase_t mb_pool2[MB_TO_SERVO_SIZE]; 

void key_callback_init(void);

static void keypad_thread_entry(void *parameter){
    keypad_t keypad = {
        .pin_row = {GPIO_PIN_0, GPIO_PIN_1, GPIO_PIN_2, GPIO_PIN_3}, 
        .pin_col = {GPIO_PIN_4, GPIO_PIN_5, GPIO_PIN_6, GPIO_PIN_7}, 
        .GPIOx = GPIOA 
    };
    key_callback_init();
    keypad_init(&keypad);
    keypad_sem = rt_sem_create("keypad_sem",1,RT_IPC_FLAG_FIFO);

    // 静态初始化：直接用地址
    rt_mb_init(&mb_to_oled, "mb_oled", mb_pool1, MB_TO_OLED_SIZE, RT_IPC_FLAG_FIFO);
    rt_mb_init(&mb_to_servo, "mb_servo", mb_pool2, MB_TO_SERVO_SIZE, RT_IPC_FLAG_FIFO);
    
    while (1)
    {

#ifdef IDLE_CNT_TEST_ENABLE
        rt_thread_delay(10000);
#endif

        keypad_update(&keypad);
        rt_thread_mdelay(20); 
    }
}

int keypad_thread_init(){
    keypad_thread = rt_thread_create("keypad_thread",
                                   keypad_thread_entry,
                                   RT_NULL,
                                   512,
                                   18,
                                   10);

    rt_thread_startup(keypad_thread);
    return 0;
}
INIT_APP_EXPORT(keypad_thread_init);


//按键功能函数
void key_fuction(uint8_t key_val){ // key_val 是 0-15 的raw值
    LOG_D("Key pressed: %d", key_val);
    if(key_val == KEY_PGDN || key_val == KEY_PGUP){
        rt_sem_take(keypad_sem,100);
    }
    rt_ubase_t msg;
    msg = (rt_ubase_t)key_val; 
    rt_mb_send(&mb_to_oled, msg); //发送到OLED线程
    rt_mb_send(&mb_to_servo, msg); //发送到舵机线程

}

//注册按键功能
void key_callback_init(){
    for(int8_t  i= 0; i < 16; i++){
        key_callback[i] = key_fuction;
    }
}