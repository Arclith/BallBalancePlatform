#ifndef OV7670_H
#define OV7670_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#define CONFIG_CLK_DIV1          0x00    //不分频
#define CONFIG_CLK_DIV2          0x01    //2倍分频
#define CONFIG_CLK_DIV4          0x03    //4倍分频

#define FRAMESIZE_320x240   FRAMESIZE_QVGA   
#define FRAMESIZE_160x120   FRAMESIZE_QQVGA
#define FRAMESIZE_640x480   FRAMESIZE_VGA

/**
 * @brief 安装驱动,初始化 OV7670 摄像头
 */
void ov7670_init(void);

/**
 * @brief 设置 OV7670 的时钟分频
 * @param div 分频系数 (CONFIG_CLK_DIV1, CONFIG_CLK_DIV2, CONFIG_CLK_DIV4)
 */
void ov7670_set_clock_divider(uint8_t div);

/**
 * @brief 开启或关闭夜间模式 (自动降帧增加曝光)
 * @param enable true: 开启, false: 关闭
 */
void ov7670_set_night_mode(bool enable);

/**
 * @brief 开启或关闭彩条测试模式
 * @param enable true: 开启, false: 关闭
 */
void ov7670_color_test(bool enable);

/**
 * @brief 设置帧大小
 * @param size 帧大小 (FRAMESIZE_320x240, FRAMESIZE_160x120, FRAMESIZE_640x480)
 */
void ov7670_framesize_set(uint8_t size);

#ifdef __cplusplus
}
#endif

#endif /* OV7670_H */