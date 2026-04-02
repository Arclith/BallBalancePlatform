#ifndef MYLIB_H
#define MYLIB_H

#include "stm32f1xx_hal.h"  // 必要 HAL 头
#include <stdint.h>
#include <math.h> // For log10

#ifdef __cplusplus
extern "C" {
#endif

// 模块可选的类型定义、结构体、宏等

#define GET_INT_DIGITS(val) ({\
    typeof(val) x = (val); \
    if (x < 0) x = -x; \
    (x == 0 ? 1 : \
    x >= 1000000000 ? 10 : \
    x >= 100000000 ? 9 : \
    x >= 10000000 ? 8 : \
    x >= 1000000 ? 7 : \
    x >= 100000 ? 6 : \
    x >= 10000 ? 5 : \
    x >= 1000 ? 4 : \
    x >= 100 ? 3 : \
    x >= 10 ? 2 : 1); \
})

void ftoa(float val,char* buf,uint8_t buf_len);
void itoa(int val,char* buf, uint8_t buf_len);

/**
 * @brief 快速整数幂计算
 * 计算 base^exp 的值
 * @param base 底数
 * @param exp 指数(非负)
 * @return int 结果
 */
int power(int base, uint8_t exp);

/**
 * @brief 浮点数幂计算
 * 支持负数指数，例如 power_float(2, -2) = 0.25
 * @param base 底数
 * @param exp 指数(可为负)
 * @return float 结果
 */
float power_float(float base, int exp);

// PID 控制器结构体定义
typedef struct {
    float kp;
    float ki;
    float kd;
    float target;
    float error_sum;
    float last_measure;
    float output_limit;  // 输出限幅 (舵机角度限制)
    float integral_limit;// 积分限幅
    float integral_range;// 积分分离阈值：当误差绝对值小于此值时才进行积分
} kpid_t;

void pid_init(kpid_t *pid, float kp, float ki, float kd, float limit);
float pid_calc(kpid_t *pid, float measure,float target,float dt_ms);
void pid_reset(kpid_t *pid);

#ifdef __cplusplus
}
#endif

#endif /* MYLIB_H */