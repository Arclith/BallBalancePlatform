#include <stm32f1xx_hal.h>
#include "ring_buff.h"
#include "shared_drive_init.h"
#include <rtthread.h>
#include <math.h>

#define DBG_TAG "mpu"
#define DBG_LVL DBG_LOG
#include <rtdbg.h>
#include "mpu6050.h"

// 灵敏度系数 (LSB/unit)
#define MPU_GYRO_LSB_250        131.0f
#define MPU_GYRO_LSB_500        65.5f
#define MPU_GYRO_LSB_1000       32.8f
#define MPU_GYRO_LSB_2000       16.4f

#define MPU_ACCEL_LSB_2         16384.0f
#define MPU_ACCEL_LSB_4         8192.0f
#define MPU_ACCEL_LSB_8         4096.0f
#define MPU_ACCEL_LSB_16        2048.0f

// 平台初始态传感器测到的重力分量
#define MPU_OFFSET_AX  1.1f
#define MPU_OFFSET_AY  0.4f

void mpu_write_reg(const uint8_t ra,const uint8_t data){
    frame_head_t frame_head = {
        .data_len = 2,
        .mode = IT,
        .device = mpu,
    };
    while(rb_write(&rb_i2c1,&frame_head,(uint8_t[]){ ra, data },my_hi2c1.i2c_sem) == -1){
        LOG_D(" mpu command full");
    };
}

static uint8_t mpu_dma_buf[14];
struct rt_semaphore mpu_sem;

void mpu_init(mpu6050_handle_t *handle)
{
    i2c_init(handle->i2c);
    
    mpu_write_reg(0x6B, 0x00); // 解除休眠
    mpu_write_reg(0x19, 0x07); // 采样率分频
    mpu_write_reg(0x1A, 0x06); // 配置DLPF
    mpu_write_reg(0x1B, handle->gyro_fs);  // 写入句柄配置的量程
    mpu_write_reg(0x1C, handle->accel_fs); // 写入句柄配置的量程
    
    rt_sem_init(&mpu_sem, "mpu_sem", 0, RT_IPC_FLAG_FIFO);
}

void mpu_set_dlpf(uint8_t dlpf)
{
    mpu_write_reg(0x1A, dlpf);
}

void mpu_set_smplrt_div(uint8_t div){
    mpu_write_reg(0x19, div);
}

void mpu_read(mpu6050_handle_t *handle, float *ax, float *ay, float *az,
                 float *gx, float *gy, float *gz, float *temp){

    // 启动DMA读取
    rt_sem_take(my_hi2c1.i2c_sem, RT_WAITING_FOREVER);
    HAL_StatusTypeDef _ret = HAL_I2C_Mem_Read_IT(&my_hi2c1.hi2c, 0x68 << 1, 0x3B, I2C_MEMADD_SIZE_8BIT, mpu_dma_buf, 14);
    rt_sem_release(my_hi2c1.i2c_sem);
    if (_ret != HAL_OK) {
        LOG_E("MPU read error %d", _ret);
        return; 
    }

    /* 等待DMA完成 */
    rt_sem_take(&mpu_sem, RT_WAITING_FOREVER);
    
    // 根据句柄中的量程动态选择系数
    float accel_sens, gyro_sens;

    switch(handle->accel_fs) {
        case MPU_ACCEL_FS_4:  accel_sens = MPU_ACCEL_LSB_4;  break;
        case MPU_ACCEL_FS_8:  accel_sens = MPU_ACCEL_LSB_8;  break;
        case MPU_ACCEL_FS_16: accel_sens = MPU_ACCEL_LSB_16; break;
        default:              accel_sens = MPU_ACCEL_LSB_2;  break;
    }

    switch(handle->gyro_fs) {
        case MPU_GYRO_FS_500:  gyro_sens = MPU_GYRO_LSB_500;  break;
        case MPU_GYRO_FS_1000: gyro_sens = MPU_GYRO_LSB_1000; break;
        case MPU_GYRO_FS_2000: gyro_sens = MPU_GYRO_LSB_2000; break;
        default:               gyro_sens = MPU_GYRO_LSB_250;  break;
    }

    // 单位换算
    if(ax != NULL) *ax = ((int16_t)((mpu_dma_buf[0] << 8) | mpu_dma_buf[1])) / accel_sens * 9.8f;
    if(ay != NULL) *ay = ((int16_t)((mpu_dma_buf[2] << 8) | mpu_dma_buf[3])) / accel_sens * 9.8f;
    if(az != NULL) *az = ((int16_t)((mpu_dma_buf[4] << 8) | mpu_dma_buf[5])) / accel_sens * 9.8f;
    if(temp != NULL) *temp = ((int16_t)((mpu_dma_buf[6] << 8) | mpu_dma_buf[7])) / 340.0f + 36.53f;
    if(gx != NULL) *gx  = ((int16_t)((mpu_dma_buf[8] << 8) | mpu_dma_buf[9])) / gyro_sens;
    if(gy != NULL) *gy  = ((int16_t)((mpu_dma_buf[10] << 8) | mpu_dma_buf[11])) / gyro_sens;
    if(gz != NULL) *gz  = ((int16_t)((mpu_dma_buf[12] << 8) | mpu_dma_buf[13])) / gyro_sens;

}




// 互补滤波系数，0.98 是经验值
// angle = alpha * (angle + gyro * dt) + (1 - alpha) * accel_angle
#define ALPHA 0.98f
float mpu_angle_x;
float mpu_angle_y;


void mpu_get_angle(mpu6050_handle_t mpu_handle ,float dt)
{
    float ax, ay, az, gx, gy, gz;
    
    // 读取处理过的（已校准偏置）的数据
    mpu_read(&mpu_handle, &ax, &ay, &az, &gx, &gy, &gz, NULL);

    // 计算加速度计得到的角度 (弧度 -> 角度)
    // atan2 返回 -pi 到 pi
     mpu_angle_x = atan2f(ay, az) * 57.29578f;
     mpu_angle_y = -atan2f(ax, sqrtf(ay*ay + az*az)) * 57.29578f;

    // 互补滤波融合
    // 陀螺仪积分: 当前角度 + 角速度(度/秒) * 时间(秒)
    mpu_angle_x  = ALPHA * (mpu_angle_x  + gx * dt) + (1.0f - ALPHA) * mpu_angle_x;
    mpu_angle_y = ALPHA * (mpu_angle_y + gy * dt) + (1.0f - ALPHA) * mpu_angle_y;
}

// DMA完成回调
void HAL_I2C_MemRxCpltCallback(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == I2C1)
    {
        rt_sem_release(&mpu_sem);
    }                                   
}