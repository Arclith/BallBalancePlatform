#include "stm32f1xx_hal.h"
#include <rtthread.h>
#include "kalman.h"
#include "mpu6050.h"
#include "shared_drive_init.h"
#include "drv_uart.h"
#include <stdbool.h>

extern my_hi2c_handle_t my_hi2c1;
mpu6050_handle_t mpu_handle = {
    .gyro_fs = MPU_GYRO_FS_500,
    .accel_fs = MPU_ACCEL_FS_2,
    .i2c = &my_hi2c1,
};

static rt_thread_t mpu_thread = RT_NULL;
bool data_analysis_mode = true;
float ax, ay, az, gx, gy, gz, temp;

#ifdef USE_KALMAN_FILTER
rt_mailbox_t mb_mpu = NULL;

void data_analyze(float f,bool mode){
    
    if(!mode) return;
    uint8_t buf[8] = {0xAA, 0xBB, 0xCC};
    rt_memcpy(buf + 3, &f, 4);
    for (int i = 0; i < 4; i++) buf[7] += buf[i + 3];
    uart_send_buf(&my_huart1, buf, 8);
}

void mpu_thread_entry(void *parameter){  
    uart_init(&my_huart1);
    mpu_init(&mpu_handle);
    mpu_set_dlpf(MPU_DLPF_5HZ);
    mb_mpu = rt_mb_create("mb_mpu", 5, RT_IPC_FLAG_FIFO);
    while (1)
    {

#ifdef IDLE_CNT_TEST_ENABLE
        rt_thread_delay(10000);
#endif

        // mpu_read(&mpu_handle, &ax, &ay, &az, &gx, &gy, &gz, &temp);
        mpu_get_angle(mpu_handle, 0.1f); // 100ms = 0.1s
        rt_mb_send(mb_mpu, 0);
        

    //    data_analyze(ax, data_analysis_mode);
        rt_thread_delay(1);
    }
}

int mpu_thread_init(){
    mpu_thread = rt_thread_create("mpu_thread",
                                   mpu_thread_entry,
                                   RT_NULL,
                                   512,
                                   15,
                                   10);

    rt_thread_startup(mpu_thread);
    return 0;
}
INIT_APP_EXPORT(mpu_thread_init);
#endif