#ifndef ENCODER_H
#define ENCODER_H

#include "stm32f1xx_hal.h"  // 必要 HAL 头
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 模块可选的类型定义、结构体、宏等

void encoder_init(void);
int16_t encoder_read(void);


#ifdef __cplusplus
}
#endif

#endif /* ENCODER_H */