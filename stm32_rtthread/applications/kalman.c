#include "kalman.h"

void kalman_pos_init(kalman_pos_t *kalman) {
    kalman->Q_x = 0.01f; // 位置的过程噪声方差
    kalman->Q_v = 0.1f;  // 速度的过程噪声方差
    kalman->R_x = 0.5f;  // 位置的测量噪声方差

    kalman->x = 0.0f;
    kalman->v = 0.0f;

    kalman->P[0][0] = 1.0f;
    kalman->P[0][1] = 0.0f;
    kalman->P[1][0] = 0.0f;
    kalman->P[1][1] = 1.0f;
}

void kalman_pos_predict(kalman_pos_t *kalman, float accel, float dt) {
    // 预测 (Predict)
    // x = x + v * dt + 0.5 * a * dt^2
    kalman->x += kalman->v * dt + 0.5f * accel * dt * dt;
    // v = v + a * dt
    kalman->v += accel * dt;
    
    // P = F * P * F^T + Q
    // F = [1 dt; 0 1]
    float P00_temp = kalman->P[0][0];
    float P01_temp = kalman->P[0][1];
    
    kalman->P[0][0] += dt * (kalman->P[1][0] + kalman->P[0][1] + dt * kalman->P[1][1]) + kalman->Q_x;
    kalman->P[0][1] += dt * kalman->P[1][1];
    kalman->P[1][0] += dt * kalman->P[1][1];
    kalman->P[1][1] += kalman->Q_v;
}

void kalman_pos_update(kalman_pos_t *kalman, float pos_m) {
    // 更新 (Update)
    // y = z - H * x
    // H = [1 0]
    float y = pos_m - kalman->x;

    // S = H * P * H^T + R
    float S = kalman->P[0][0] + kalman->R_x;

    // K = P * H^T / S
    float K0 = kalman->P[0][0] / S;
    float K1 = kalman->P[1][0] / S;

    // x = x + K * y
    kalman->x += K0 * y;
    kalman->v += K1 * y;

    // P = (I - K * H) * P
    float P00_temp = kalman->P[0][0];
    float P01_temp = kalman->P[0][1];

    kalman->P[0][0] -= K0 * P00_temp;
    kalman->P[0][1] -= K0 * P01_temp;
    kalman->P[1][0] -= K1 * P00_temp;
    kalman->P[1][1] -= K1 * P01_temp;
}
