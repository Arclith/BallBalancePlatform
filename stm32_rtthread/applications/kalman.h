#ifndef KALMAN_H
#define KALMAN_H

//#define USE_KALMAN_FILTER // 定义此宏以启用卡尔曼滤波及相关MPU线程

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    float x; // 位置
    float v; // 速度
    
    float P[2][2]; // 误差协方差矩阵
    float Q_x; // 位置的过程噪声方差
    float Q_v; // 速度的过程噪声方差
    float R_x; // 位置的测量噪声方差
} kalman_pos_t;

void kalman_pos_init(kalman_pos_t *kalman);
void kalman_pos_predict(kalman_pos_t *kalman, float accel, float dt);
void kalman_pos_update(kalman_pos_t *kalman, float pos_m);

#ifdef __cplusplus
}
#endif

#endif // KALMAN_H
