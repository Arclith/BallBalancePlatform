#ifndef MPU6050_H
#define MPU6050_H

#include "stm32f1xx_hal.h"  // 必要 HAL 头
#include <stdint.h>
#include "shared_drive_init.h"

#ifdef __cplusplus
extern "C" {
#endif

// 陀螺仪量程 (GYRO_CONFIG 0x1B)
#define MPU_GYRO_FS_250         0x00
#define MPU_GYRO_FS_500         0x08
#define MPU_GYRO_FS_1000        0x10
#define MPU_GYRO_FS_2000        0x18

// 加速度计量程 (ACCEL_CONFIG 0x1C)
#define MPU_ACCEL_FS_2          0x00    
#define MPU_ACCEL_FS_4          0x08
#define MPU_ACCEL_FS_8          0x10
#define MPU_ACCEL_FS_16         0x18


// 数字低通滤波器配置 (CONFIG 0x1A)
#define MPU_DLPF_260HZ          0x00 //此参数下陀螺仪内部采样率为8khz,其余为1khz
#define MPU_DLPF_184HZ          0x01
#define MPU_DLPF_94HZ           0x02
#define MPU_DLPF_44HZ           0x03
#define MPU_DLPF_21HZ           0x04
#define MPU_DLPF_10HZ           0x05
#define MPU_DLPF_5HZ            0x06

// MPU6050 句柄
typedef struct {
    uint8_t gyro_fs;         // 陀螺仪量程配置 (如 MPU_GYRO_FS_500)
    uint8_t accel_fs;        // 加速度计量程配置 (如 MPU_ACCEL_FS_2)
    my_hi2c_handle_t *i2c;          // I2C 句柄指针
} mpu6050_handle_t;


/**
 * @brief 初始化 MPU6050 设备句柄并做基本配置
 *
 * 该函数应完成对 `mpu6050_handle_t` 中成员的必需初始化，
 * 并根据 handle 中的量程配置（gyro_fs / accel_fs）或默认值对 MPU6050 寄存器做初始设置。
 *
 * @param handle 指向已分配且含有 i2c 指针的 MPU 句柄，不能为 NULL。
 *
 */
void mpu_init(mpu6050_handle_t *handle);


/**
 * @brief 设置 MPU6050 内部数字低通滤波器 (DLPF)
 *
 * DLPF 用于对陀螺仪与加速度计输出进行带限滤波，降低高频噪声。参数 dlpf 应使用本头文件
 * 中定义的 MPU_DLPF_xHZ 宏值（例如 MPU_DLPF_94HZ）。
 *
 * @param dlpf DLPF 配置宏（MPU_DLPF_260HZ / MPU_DLPF_184HZ / ... / MPU_DLPF_5HZ）
 */
void mpu_set_dlpf(uint8_t dlpf);


/**
 * @brief 读取 MPU6050 的加速度、陀螺仪和温度数据（以物理单位返回）
 *
 * @param handle 已初始化的 MPU 句柄
 * @param ax 指向 float，用于接收 X 轴加速度（单位：g）
 * @param ay 指向 float，用于接收 Y 轴加速度（单位：g）
 * @param az 指向 float，用于接收 Z 轴加速度（单位：g）
 * @param gx 指向 float，用于接收 X 轴角速度（单位：°/s）
 * @param gy 指向 float，用于接收 Y 轴角速度（单位：°/s）
 * @param gz 指向 float，用于接收 Z 轴角速度（单位：°/s）
 * @param temp 指向 float，用于接收温度（单位：°C）
 *
 * 说明：
 * - 函数内部从 MPU 的原始寄存器读取有符号 16bit 值，并根据当前量程（GYRO/ACCEL）换算为物理单位。
 * - 如果不需要某个输出，可以将对应指针传入 NULL，函数将跳过该输出的赋值。
 */
void mpu_read(mpu6050_handle_t *handle, float *ax, float *ay, float *az,
                 float *gx, float *gy, float *gz, float *temp);


/**
 * @brief 设置 MPU6050 的采样率分频器（SMPLRT_DIV 寄存器）
 *
 * SMPLRT_DIV 寄存器定义了从陀螺仪输出率到最终采样率的分频：
 *
 *     Sample Rate = Output_Rate / (1 + SMPLRT_DIV)
 *     其中陀螺仪Output_Rate受DLPF配置影响,加速度计固定为1khz
 *          
 * @param div SMPLRT_DIV 寄存器写入值（0-255）
 * 注意：
 * - 推荐在调用本函数后，若代码依赖于固定采样率，应在主循环或配置处保存并使用上述公式来计算实际采样频率。
 * - 将过大的采样率设置用于后续滤波或控制环路时，需要保证 MCU 有足够处理能力。
 */
void mpu_set_smplrt_div(uint8_t div);

extern float mpu_angle_x;
extern float mpu_angle_y;

void mpu_get_angle(mpu6050_handle_t mpu_handle,float dt);

#ifdef __cplusplus
}
#endif

#endif /* MPU6050_H */
