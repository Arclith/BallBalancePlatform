/*
 * Copyright (c) 2006-2025, RT-Thread Development Team
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Change Logs:
 * Date           Author       Notes
 * 2025-12-13     RT-Thread    first version
 */

#include <rtthread.h>
#include "drv_uart.h"

#define DBG_TAG "main"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include "oled.h"

#include "stm32f1xx_hal.h"   // 或你的 HAL 头
#include <stdint.h>
#include "mpu6050.h"
#include "servo.h"
#include "encoder.h"
#include "mylib.h"


uint32_t board_flash_size_bytes(void)
{
    // FLASH_SIZE_DATA_REGISTER 在 stm32f1xx_hal_flash_ex.h 中定义为 0x1FFFF7E0U
    uint16_t flash_kb = *(uint16_t *)FLASH_SIZE_DATA_REGISTER;
    if (flash_kb == 0)
    {
        // 出错 / 未知值，返回 0 以示失败
        return 0;
    }
    return (uint32_t)flash_kb * 1024U;
}

/* 调试打印示例（RT-Thread） */
void print_flash_info(void)
{
    uint32_t bytes = board_flash_size_bytes();
    if (bytes == 0)
    {
        LOG_W("Flash size: unknown (read 0)");
    }
    else
    {
        LOG_I("Flash size: %u bytes (%u KB)", bytes, (unsigned)(bytes / 1024U));
    }
}




int main(void){  

    rt_thread_mdelay(1000000);
    return RT_EOK;
}
