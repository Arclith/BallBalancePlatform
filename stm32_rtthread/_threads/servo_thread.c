#include "stm32f1xx_hal.h"
#include <rtthread.h>
#include "servo.h"
#include "keypad.h"
#include "mylib.h"
#include "encoder.h"
#include "oled.h"
#include <stdbool.h>
#include "mpu6050.h" // 引入 MPU 头文件以获取角度
#include "drv_uart.h"
#include "kalman.h"
#include <math.h>

// USE_KALMAN_FILTER is now defined in kalman.h

#define DBG_TAG "servo_thread"
#define DBG_LVL DBG_INFO

#include <rtdbg.h>

float p = 7.9f;
float i = 0.106f;
float d = 2.0f;
float kalman_q = 0.002f; // Kalman 滤波器参数 Q
float kalman_r = 0.5f;  // Kalman 滤波器参数 R

extern rt_sem_t keypad_sem;
extern rt_mq_t mq_xy;
extern my_huart_handle_t my_huart1;
extern bool data_analysis_mode;
extern rt_mailbox_t mb_mpu;

static rt_thread_t servo_thread = RT_NULL;
extern float fx, fy;

uint8_t count = 0, byte = 0;

// 定义两套 PID：位置环和角度环
// X 轴
kpid_t pid_pos_x, pid_angle_x,pid_speed_x;
// Y 轴
kpid_t pid_pos_y, pid_angle_y,pid_speed_y;

void pid_debug(bool t,kpid_t *pidx,kpid_t *pidy){
    static int16_t last_encoder = 0;
    if(!t){
        last_encoder = encoder_read() ;
        return;
    }
    float *obj = NULL;
    char buf[8];
    switch(count){
        case 0:obj = &p;break;
        case 1:obj = &i;break;
        case 2:obj = &d;break;
    }
    ftoa(*obj,buf,7);
    uint8_t digit = GET_INT_DIGITS(*obj);       
    int16_t derta_encoder = encoder_read() - last_encoder;
    last_encoder = encoder_read();
    if(derta_encoder != 0){
        if(derta_encoder < 0 && buf[0] == '0' && byte == 0) return; 
        if(byte < digit) *obj += (float)derta_encoder * power(10, digit - byte - 1);
        if(byte == digit) return;
        if(byte > digit) *obj += (float)derta_encoder / power(10, byte - digit);
    }
    if(pidx != NULL){
        pidx->kp = p;
        pidx->ki = i;
        pidx->kd = d; 
    }
    if(pidy != NULL){
        pidy->kp = p;
        pidy->ki = i;
        pidy->kd = d; 
    }

}

bool flag = 0;
float angle;

void servo_test(uint8_t port){
    angle = (float)encoder_read() * 2  ;
    servo_set_angle(angle,port);
}

bool test_mode = false, debug_mode = true;
uint8_t set_offset_mode = 0;

void servo_thread_entry(void *_parameter){
    rt_base_t msg;
    extern struct rt_mailbox mb_to_servo;
    servo_init();
    pid_init(&pid_speed_x,7.9f,0.136,2,80);
    pid_init(&pid_speed_y,7.9f,0.056,2,80);
    pid_init(&pid_pos_x,0.056,0,10,200);
    pid_init(&pid_pos_y,0.056,0,10,200);
    encoder_init();
    uart_init(&my_huart1);
    LOG_I("servo thread start");
    //ffset set: offset_x=4.66215, offset_y=-3.9670
    float offset_x = 0.6f; 
    float offset_y = 1.0f;
    uint16_t offset_count_x = 0, offset_count_y = 0;
    while (1){

#ifdef IDLE_CNT_TEST_ENABLE
        rt_thread_delay(10000);
#endif  
        uint8_t recv_buf[8] = {0};
        if(set_offset_mode){
        pid_speed_x.integral_range = 2.0f; // 速度小于15时才积分
        pid_speed_y.integral_range = 2.0f;

            if(rt_mq_recv(mq_xy, recv_buf,8, 100) == RT_EOK){
                float tx = *((float*)recv_buf);
                float ty = *((float*)(recv_buf + 4));
                static float last_tx = 0, last_ty = 0;
                static rt_tick_t last_tick = 0;
                if(tx != -1 && ty != -1){
                    rt_tick_t now = rt_tick_get();
                    float dt = (float)(now - last_tick);
                    if(dt > 200 || dt <= 0) dt = 20;

                    float vx = tx - last_tx;
                    float vy = ty - last_ty;

                    float angle_x = -pid_calc(&pid_speed_x, vx, 0, dt);
                    float angle_y = pid_calc(&pid_speed_y, vy, 0, dt);
                    
                    servo_set_angle(angle_x, 0);
                    servo_set_angle(angle_y, 1);
                    
                    if(fabs(vx) < 0.5f) offset_count_x++;
                    else offset_count_x = 0;

                    if(fabs(vy) < 0.5f) offset_count_y++;
                    else offset_count_y = 0;

                    if(offset_count_x > 50 && offset_count_y > 50){
                        offset_x = angle_x;
                        offset_y = angle_y;
                        char bufx[8], bufy[8];
                        ftoa(offset_x,bufx,8);
                        ftoa(offset_y,bufy,8);
                        LOG_I("Offset set: offset_x=%s, offset_y=%s", bufx, bufy);
                        
                        pid_reset(&pid_speed_x);
                        pid_reset(&pid_speed_y);
                        offset_count_x = 0;
                        offset_count_y = 0;
                    }

                    last_tick = now;

                    uint8_t head[] = {0xAA, 0xAA};
                    uart_send_buf(&my_huart1, head, 2);
                        uart_send_float(&my_huart1, 0.0f); // 将 vx 改为 target_vx 方便观察跟踪情况
                        uart_send_float(&my_huart1, 0.0f);
                        uart_send_float(&my_huart1, vx);        // 实际速度
                        uart_send_float(&my_huart1, vy);  
                        uart_send_float(&my_huart1, tx);
                        uart_send_float(&my_huart1, ty);
                        uart_send_float(&my_huart1, angle_x);
                        uart_send_float(&my_huart1, angle_y);
                }
                last_tx = tx;
                last_ty = ty;
            } 

        }else{
            pid_speed_x.ki = 0;
            pid_speed_y.ki = 0;
            static kalman_pos_t kalman_x, kalman_y;
            
            static float angle_x = 0, angle_y = 0;
            static bool has_ball = false;
            static rt_tick_t last_tick1 = 0,last_tick2 = 0;
    #ifndef USE_KALMAN_FILTER
            static float last_tx = 0, last_ty = 0;
    #endif

    #ifdef USE_KALMAN_FILTER
            if(rt_mb_recv(mb_mpu,&msg,1) == RT_EOK){
                float accel_x = 9800.0f * sinf(mpu_angle_x * 0.0174533f);
                float accel_y = 9800.0f * sinf(mpu_angle_y * 0.0174533f);
                float dt = (float)(rt_tick_get() - last_tick2);
                if (dt <= 0) dt = 1; // 防止除0错误
                float dt_s = dt / 1000.0f;

                kalman_pos_predict(&kalman_x, accel_x, dt_s);
                kalman_pos_predict(&kalman_y, accel_y, dt_s);
                last_tick2 = rt_tick_get();
            }
    #endif
            if(rt_mq_recv(mq_xy, recv_buf,8, 100) == RT_EOK){
                float tx = *((float*)recv_buf);
                float ty = *((float*)(recv_buf + 4));
                if(tx != -1 && ty != -1 && test_mode){
                    if(!has_ball){
    #ifdef USE_KALMAN_FILTER
                        // 上球瞬间：初始化滤波器状态
                        kalman_pos_init(&kalman_x);
                        kalman_pos_init(&kalman_y);
                        kalman_x.x = tx;
                        kalman_y.x = ty;
    #else
                        last_tx = tx;
                        last_ty = ty;
    #endif

                        last_tick1 = rt_tick_get();
                        angle_x = offset_x;
                        angle_y = offset_y;
                        has_ball = true;
                    } else {
                        float dt = (float)(rt_tick_get() - last_tick1);
                        if (dt <= 0) dt = 1; // 防止除0错误

    #ifdef USE_KALMAN_FILTER
                        // --- Kalman 滤波器算法 ---
                        kalman_x.Q_x = kalman_q;
                        kalman_x.Q_v = kalman_q * 10.0f;
                        kalman_x.R_x = kalman_r;
                        kalman_y.Q_x = kalman_q;
                        kalman_y.Q_v = kalman_q * 10.0f;
                        kalman_y.R_x = kalman_r;
                        
                        // 将 dt 转换为秒，以符合物理单位 (速度: 像素/秒, 加速度: 像素/秒^2)
                        float dt_s = dt / 1000.0f;
                        

                        kalman_pos_update(&kalman_x, tx);
                        kalman_pos_update(&kalman_y, ty);

                        // 使用滤波后的位置和速度
                        float x_est = kalman_x.x;
                        float y_est = kalman_y.x;
                        // 将速度转换回 像素/ms，以兼容原有的 PID 参数
                        float vx = kalman_x.v / 1000.0f;
                        float vy = kalman_y.v / 1000.0f;
    #else
                        float x_est = tx;
                        float y_est = ty;
                        // 简单的差分计算速度
                        float vx = tx - last_tx;
                        float vy = ty - last_ty;
                        last_tx = tx;
                        last_ty = ty;
    #endif

                        // 1. 位置环 (外环)：根据位置误差计算目标速度
                        // 使用 Kalman 滤波后的位置 x_est, y_est

                        float target_vx = pid_calc(&pid_pos_x, x_est,124.0f, dt);
                        float target_vy = pid_calc(&pid_pos_y, y_est,120.0f, dt);
                        
                        // 使用滤波后的速度 vx, vy 进行反馈
                        angle_x =  - pid_calc(&pid_speed_x, vx,target_vx, dt) + offset_x;
                        angle_y =  pid_calc(&pid_speed_y, vy,target_vy, dt) + offset_y;
                        
                        servo_set_angle(angle_x, 0); // 设置 X 轴舵机角度
                        servo_set_angle(angle_y, 1); // 
                        last_tick1 = rt_tick_get();
                        if(data_analysis_mode) {
                            uint8_t head[] = {0xAA, 0xAA};
                            uart_send_buf(&my_huart1, head, 2);
                            // 外环调试：发送目标速度和实际速度
                            uart_send_float(&my_huart1, target_vx); // 将 vx 改为 target_vx 方便观察跟踪情况
                            uart_send_float(&my_huart1, target_vy);
                            uart_send_float(&my_huart1, vx);        // 实际速度
                            uart_send_float(&my_huart1, vy);  
                            uart_send_float(&my_huart1, tx);
                            uart_send_float(&my_huart1, ty);
                            uart_send_float(&my_huart1, angle_x);
                            uart_send_float(&my_huart1, angle_y);
                        }
                    }
                }else if(tx == -1 || ty == -1) {
                    has_ball = false;
                    servo_set_angle(offset_x, 0); 
                    servo_set_angle(offset_y, 1); 
                }
            } 
        } 
        //接收按键消息
        if(rt_mb_recv(&mb_to_servo,&msg,0) == RT_EOK){
            rt_sem_take(keypad_sem,1000);
            if(oled_state == debug) {
                debug_mode = true;
                switch (msg){
                    case KEY_UP:
                        count = (count + 2) % 3;
                        break;
                    case KEY_DOWN:
                        count = (count + 1) % 3;
                        break;
                    case KEY_RIGHT:
                        byte++;
                        break;
                    case KEY_LEFT:
                        byte--;
                        break;
                    case KEY_OK:
                        set_offset_mode = !set_offset_mode;
                        servo_on_off(0,set_offset_mode);
                        servo_on_off(1,set_offset_mode);
                        break;
                }
            }
            if(oled_state == menu){
                debug_mode = false;
                switch (msg){
                    case KEY_OK:
                        test_mode = !test_mode;
                        servo_on_off(0,test_mode);
                        servo_on_off(1,test_mode);
                        flag = 1;
                        break;
                }
            }
            rt_sem_release(keypad_sem);
        }
      //  pid_debug(debug_mode,&pid_pos_x,&pid_pos_y);
      //  pid_debug(set_offset_mode,&pid_speed_x,NULL);
    //  servo_test(1);
    }
}

int servo_thread_init(){
    servo_thread = rt_thread_create("servo_thread",
                                servo_thread_entry,
                                   RT_NULL,
                                   1024,
                                   7,
                                   10);
    rt_thread_startup(servo_thread);
    return 0;
}
INIT_APP_EXPORT(servo_thread_init);

