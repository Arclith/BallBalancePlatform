#ifndef __oled_H__
#define __oled_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"


enum{
    menu,
    debug,
    mpu_data,
    esp_info  
};
#define PAGE_COUNT 4
extern uint8_t oled_state;

/**
 * @brief OLED 寻址模式枚举
 */
typedef enum {
    OLED_ADDR_MODE_HORIZONTAL = 0x00, // 水平寻址：写满一行自动换到下一行起始处
    OLED_ADDR_MODE_VERTICAL   = 0x01, // 垂直寻址：写满一列自动换到下一列起始处
    OLED_ADDR_MODE_PAGE       = 0x02  // 页寻址（默认）：写满一页后指针重置到该页起始，不自动换行
} oled_addr_mode_t;

/**
 * @brief 初始化 OLED 屏幕
 * @note  包含 I2C 初始化、显示参数配置及清屏操作
 */
void oled_init();

/**
 * @brief 设置 OLED 的内存寻址模式
 * @param mode 寻址模式 
 */
void oled_set_addr_mode(oled_addr_mode_t mode);

/**
 * @brief 设置 OLED 的对比度
 * @param contrast 对比度值 (0~255) 
 */
void oled_set_contrast(uint8_t contrast);

/**
 * @brief 设置 OLED 反色模式
 * @param on 1: 开启反色 (亮黑对调), 0: 正常显示
 */
void oled_set_inverse(uint8_t on);

/**
 * @brief 设置光标位置
 * @param row 行号 (1~8)
 * @param col 列位置 (1~21, 基于6x8字体计算得出)
 */
void oled_set_cursor(uint8_t row, uint8_t col);

/**
 * @brief 在当前位置写入字符串
 * @param str 要显示的字符串指针
 */
void oled_write_str(const uint8_t *str);

/**
 * @brief 在指定位置写入字符串
 * @param row 行号 (1~8)
 * @param col 列号 (1~21)
 * @param str 字符串指针
 */
void oled_write_str_at(uint8_t row, uint8_t col, const uint8_t *str);

/**
 * @brief 在当前位置写入整数
 * @param num 要显示的整数
 * @param width 整数最大宽度
 */
void oled_write_int(int num, uint8_t width);

/**
 * @brief 在指定位置写入整数
 * @param row 行号
 * @param col 列号
 * @param num 整数值
 * @param width 整数最大宽度
 */
void oled_write_int_at(uint8_t row, uint8_t col, int num, uint8_t width);

/**
 * @brief 在当前位置写入浮点数
 * @param val 浮点数值
 * @param width 浮点数显示宽度(包含小数点)
 */
void oled_write_float(float val, uint8_t width);

/**
 * @brief 在指定位置写入浮点数
 * @param row 行号
 * @param col 列号
 * @param val 浮点数值
 * @param width 浮点数显示宽度(包含小数点)
 */
void oled_write_float_at(uint8_t row, uint8_t col, float val, uint8_t width);

/**
 * @brief 全屏清屏
 */
void oled_clear();

/**
 * @brief 清除指定行中指定长度的文字
 * @param row 行号
 * @param col 起始列
 * @param len 清除长度 (字符个数)
 */
void oled_clear_at(uint8_t row, uint8_t col, uint8_t len);

/**
 * @brief 清除整行
 * @param row 行号 (1~8)
 */
void oled_clear_line(uint8_t row);

/**
 * @brief 清除指定位置的一个字符
 * @param row 行号
 * @param col 列号
 */
void oled_clear_word(uint8_t row, uint8_t col);

void oled_write_tri(uint8_t row,uint8_t col);   

#ifdef __cplusplus
}
#endif

#endif /* __oled_H__ */