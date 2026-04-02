#include "stm32f1xx_hal.h"
#include <rtthread.h>
#include "oled.h"
#include "keypad.h"
#include <stdbool.h>

#define DBG_TAG "oled_thread"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>

static rt_thread_t oled_thread = RT_NULL;
extern float p,i,d;
extern bool test_mode;
extern rt_sem_t keypad_sem;
extern float ax, ay, az, temp;
extern bool data_analysis_mode;

extern struct rt_mailbox mb_to_oled;
extern float cam_fps, lcd_fps, fx, fy;
extern float angle;
extern float kalman_q;
extern float kalman_r;
extern uint8_t set_offset_mode;

uint8_t oled_state = menu;
static rt_tick_t idle_tick = 0;
static uint32_t tick = 0;

float cpu_load = 100.0f;

void cpu_load_cal(void){
    static rt_tick_t last_tick = 0;
    static uint32_t cnt = 0;
    const uint32_t max_cnt = 465726;
    if(rt_tick_get() - last_tick >= 1000){
        LOG_D("max_cnt:%d",cnt);
        cpu_load = 100.0f - (cnt * 100.0f / max_cnt);
        cnt = 0;
        last_tick = rt_tick_get();
    }
    cnt++;
}

static void oled_thread_entry(void *parameter)
{
    oled_init();
    uint8_t const_flag = 0;
    rt_ubase_t msg;
    rt_thread_idle_sethook(cpu_load_cal);
    uint8_t x = 3, y = 4; 

    while (1)
    {   

#ifdef IDLE_CNT_TEST_ENABLE
        rt_thread_delay(10000);
#endif

        //接收按键消息
        if(rt_mb_recv(&mb_to_oled,&msg,0) == RT_EOK){
            switch (msg){
                case KEY_PGUP: 
                oled_state = (oled_state + 1) % PAGE_COUNT;
                 const_flag = 0; 
                 oled_clear();
                 rt_sem_release(keypad_sem); break;
                case KEY_PGDN: 
                oled_state = (oled_state + PAGE_COUNT - 1) % PAGE_COUNT;
                 const_flag = 0; oled_clear();
                rt_sem_release(keypad_sem); break;
            }
            if(oled_state == debug){
                switch (msg){
                    case KEY_RIGHT: oled_clear_word(x,y); y++; oled_write_tri(x,y); break;
                    case KEY_LEFT: oled_clear_word(x,y); y--; oled_write_tri(x,y); break;
                    case KEY_UP: oled_clear_word(x,y); x -= 2; oled_write_tri(x,y); break;
                    case KEY_DOWN: oled_clear_word(x,y); x += 2; oled_write_tri(x,y);break;
                }
            }
            if(oled_state == mpu_data){
                switch (msg){
                    case KEY_OK: data_analysis_mode = !data_analysis_mode; break;
                }
            }

        }


        switch (oled_state){
            case debug:
                if(const_flag == 0){
                    oled_write_str_at(1,9,"debug");
                    oled_write_str_at(2,1,"p:");
                    oled_write_str_at(4,1,"i:");
                    oled_write_str_at(6,1,"d:");
                    oled_write_str_at(8,1,"set_offset_mode:");
                    oled_write_tri(x,y);
                    const_flag = 1;
                }
                oled_write_float_at(2,4,p,8);
                oled_write_float_at(4,4,i,8);
                oled_write_float_at(6,4,d,8);
                oled_write_int_at(8,19,set_offset_mode ? 1 : 0,1);
                break;
            case menu:
                if(const_flag == 0){
                    oled_write_str_at(1,9,"menu");
                    oled_write_str_at(2,1,"angle:");
                    oled_write_str_at(4,1,"CPU_load:");
                    oled_write_str_at(4,14,"%");
                    oled_write_str_at(6,1,"servo_test:");
                    const_flag = 1;
                }
                oled_write_float_at(2,8,angle,6);
                oled_write_float_at(4,10,cpu_load,4);
                oled_write_int_at(6,13,test_mode ? 1 : 0,1);
                break;
            case mpu_data:
                if(const_flag == 0){
                    oled_write_str_at(1,7,"mpu_data");
                    oled_write_str_at(2,1,"ax:");
                    oled_write_str_at(3,1,"ay:");
                    oled_write_str_at(4,1,"az:");
                    oled_write_str_at(5,1,"temp:");
                    oled_write_str_at(6,1,"data analysis:");
                    const_flag = 1;
                }
                oled_write_float_at(2,4,ax,6);
                oled_write_float_at(3,4,ay,6);
                oled_write_float_at(4,4,az,6);
                oled_write_float_at(5,6,temp,6);
                oled_write_int_at(6,17,data_analysis_mode,1);
                break;
            case esp_info:
                if(const_flag == 0){
                    oled_write_str_at(1,7,"esp_info");
                    oled_write_str_at(2,1,"fps");
                    oled_write_str_at(3,1,"cam:");
                    oled_write_str_at(3,12,"lcd:");
                                        oled_write_str_at(5,1,"x:");
                    oled_write_str_at(7,1,"y:");
                    const_flag = 1;
                }
                oled_write_float_at(3,6,cam_fps,5);
                oled_write_float_at(3,17,lcd_fps,5);
                oled_write_float_at(5,4,fx,7);
                oled_write_float_at(7,4,fy,7);
                break;
        }
    rt_thread_delay(200);
    }
}

int oled_thread_init(void)
{
    oled_thread = rt_thread_create("oled_thread",
                                   oled_thread_entry,
                                   RT_NULL,
                                   1024,
                                   20,
                                   10);

    rt_thread_startup(oled_thread);
    return 0;
}
INIT_APP_EXPORT(oled_thread_init); 


