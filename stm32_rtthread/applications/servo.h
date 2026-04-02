#ifndef SERVO_H
#define SERVO_H

#include "stm32f1xx_hal.h"  // 必要 HAL 头
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 模块可选的类型定义、结构体、宏等

void servo_init(void);
void servo_set_angle(float angle,uint8_t port);
void servo_set_angle_smooth(float angle,uint8_t port);
void servo_on_off(uint8_t port,uint8_t on_off);

#ifdef __cplusplus
}
#endif

#endif /* SERVO_H */