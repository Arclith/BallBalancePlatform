#include "stm32f1xx_hal.h"
#include "mylib.h"



void ftoa(float val,char* buf,uint8_t buf_len){
    uint8_t j=0;
    if(val < 0){
        buf[j++] ='-';
        val = -val;
    }
    uint16_t int_part =(int)val;
    float float_part = val - int_part;
    char temp[buf_len];
    uint8_t i=0;
    if(int_part == 0){
        buf[j++] = '0';
    }else{
        while(int_part != 0){
            temp[i++] = (int_part %10) + '0';
            int_part /=10;
        }
    }
    while(i > 0 && j < buf_len - 2) buf[j++] = temp[--i];
    buf[j++] = '.';
    while(j < buf_len - 1){
        float_part *=10;
        buf[j++] = (int)float_part  + '0';
        float_part -= (int)float_part;
    }
    buf[j] = '\0';
}


void itoa(int val,char* buf, uint8_t buf_len){
    uint8_t j=0;
    if(val < 0){
        buf[j++] ='-';
        val = -val;
    }
    char temp[buf_len];
    uint8_t i=0;
    if(val == 0){
        buf[j++] = '0';
    }else{
        while(val != 0){
            temp[i++] = (val % 10) + '0';
            val /= 10;
        }
    }
    while(i > 0 && j < buf_len - 1) buf[j++] = temp[--i];
    buf[j] = '\0';
}

int power(int base, uint8_t exp) {
    int res = 1;
    while (exp > 0) {
        if (exp % 2 == 1) res *= base;
        base *= base;
        exp /= 2;
    }
    return res;
}


float power_float(float base, int exp) {
    float res = 1.0f;
    int e = exp;
    if (e < 0) e = -e; // 取指数绝对值
    
    while (e > 0) {
        if (e % 2 == 1) res *= base;
        base *= base;
        e /= 2;
    }
    
    if (exp < 0) return 1.0f / res;
    return res;
}

void pid_init(kpid_t *pid, float kp, float ki, float kd, float limit) {
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    pid->error_sum = 0;
    pid->last_measure = 0;
    pid->output_limit = limit;
    pid->integral_limit = limit * 0.5f; // 经验值：积分限幅通常比总输出小一点
    pid->integral_range = 10000.0f; // 默认很大，相当于不开启分离
}

void pid_reset(kpid_t *pid) {
    pid->error_sum = 0;
    pid->last_measure = 0;
}

float pid_calc(kpid_t *pid, float measure,float target,float dt_ms) {
    pid->target = target;
    float error = pid->target - measure;

    if(fabs(error) > pid->integral_range){
        // 积分分离：误差较大时不积分，清除误差累计
        pid->error_sum = 0;
    }else{
        if(pid->ki != 0) pid->error_sum += error * dt_ms;
        // 积分抗饱和
        if (pid->error_sum > pid->integral_limit) pid->error_sum = pid->integral_limit;
        if (pid->error_sum < -pid->integral_limit) pid->error_sum = -pid->integral_limit;
    }

    float output = pid->kp * error 
                 + pid->ki * pid->error_sum 
                 - pid->kd * (measure - pid->last_measure) / dt_ms;

    pid->last_measure = measure;

    // 输出限幅
    if (output > pid->output_limit) output = pid->output_limit;
    if (output < -pid->output_limit) output = -pid->output_limit;
    
    return output;
}


