#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_camera.h"      
#include "my_ov7670.h"
#include "driver/gpio.h"
#include "esp_timer.h" 
#include "tft.h"
#include "esp_heap_caps.h"
#include <assert.h>
#include "typedef.h"
#define CONFIG_ESP32S3_VECTOR_ENABLE 1

// 使用 GCC 向量扩展，128-bit 向量（4 x 32-bit）用于并行处理 4 个像素
typedef uint32_t v4u32 __attribute__((vector_size(16)));

static const char *TAG = "cam_task";
#define LOG_LOCAL_LEVEL ESP_LOG_DEBUG
#include "esp_log.h"

volatile uint8_t *binary = NULL;
volatile uint8_t *crop_buf = NULL;
QueueHandle_t xMailbox = NULL;
extern QueueHandle_t queue_fb;
extern volatile bool Completion;
SemaphoreHandle_t sem_cam = NULL;

static void cal_frame_rate(){

    static uint32_t frame_count = 0;
    static uint64_t start_time = 0;
    
    if (frame_count == 0) {
        start_time = esp_timer_get_time();
    }
    
    frame_count++;
    
    if (frame_count % 30 == 0) {
        uint64_t current_time = esp_timer_get_time();
        uint64_t elapsed_us = current_time - start_time;
        float fps = (frame_count * 1000000.0f) / elapsed_us;
        uint8_t buf[9];
        buf[0] = 1;
        memcpy(buf+1,&fps,4);
        xQueueSend(xMailbox, buf, 0); 
      // ESP_LOGI(TAG,"fps:%f",fps);
                
        // 重置计数器
        frame_count = 0;
        start_time = current_time;
    }
}

void rgb565_to_binary(uint16_t *img, uint16_t *binary, int width, int height, uint8_t threshold) {
    int total = width * height;
    int i = 0;
    // 每次处理 8 个像素（128-bit 向量，8 x 16-bit），系数基于 RGB565 直接计算
    typedef uint16_t v8u16 __attribute__((vector_size(16)));
    const v8u16 cr = {630,630,630,630,630,630,630,630};
    const v8u16 cg = {608,608,608,608,608,608,608,608};
    const v8u16 cb = {240,240,240,240,240,240,240,240};
    for(; i + 7 < total; i += 8) {
        uint16_t p0 = img[i+0];
        uint16_t p1 = img[i+1];
        uint16_t p2 = img[i+2];
        uint16_t p3 = img[i+3];
        uint16_t p4 = img[i+4];
        uint16_t p5 = img[i+5];
        uint16_t p6 = img[i+6];
        uint16_t p7 = img[i+7];

        // 字节交换
        p0 = (p0 >> 8) | (p0 << 8);
        p1 = (p1 >> 8) | (p1 << 8);
        p2 = (p2 >> 8) | (p2 << 8);
        p3 = (p3 >> 8) | (p3 << 8);
        p4 = (p4 >> 8) | (p4 << 8);
        p5 = (p5 >> 8) | (p5 << 8);
        p6 = (p6 >> 8) | (p6 << 8);
        p7 = (p7 >> 8) | (p7 << 8);

        uint16_t rr[8], gg[8], bb[8];
        rr[0] = (p0 >> 11) & 0x1F; rr[1] = (p1 >> 11) & 0x1F; rr[2] = (p2 >> 11) & 0x1F; rr[3] = (p3 >> 11) & 0x1F;
        rr[4] = (p4 >> 11) & 0x1F; rr[5] = (p5 >> 11) & 0x1F; rr[6] = (p6 >> 11) & 0x1F; rr[7] = (p7 >> 11) & 0x1F;

        gg[0] = (p0 >> 5) & 0x3F; gg[1] = (p1 >> 5) & 0x3F; gg[2] = (p2 >> 5) & 0x3F; gg[3] = (p3 >> 5) & 0x3F;
        gg[4] = (p4 >> 5) & 0x3F; gg[5] = (p5 >> 5) & 0x3F; gg[6] = (p6 >> 5) & 0x3F; gg[7] = (p7 >> 5) & 0x3F;

        bb[0] = p0 & 0x1F; bb[1] = p1 & 0x1F; bb[2] = p2 & 0x1F; bb[3] = p3 & 0x1F;
        bb[4] = p4 & 0x1F; bb[5] = p5 & 0x1F; bb[6] = p6 & 0x1F; bb[7] = p7 & 0x1F;

        v8u16 vr = *(v8u16*)rr;
        v8u16 vg = *(v8u16*)gg;
        v8u16 vb = *(v8u16*)bb;

        v8u16 sum = vr * cr + vg * cg + vb * cb;
        sum = sum >> 8;
        uint16_t out[8];
        *(v8u16*)out = sum;
        // 二值化并写回
        for(int j = 0; j < 8; j++) {
            binary[i + j] = (out[j] >= threshold) ? 0xFFFF : 0x0000;
        }
    }

    // 处理尾部像素
    for(; i < total; i++) {
        uint16_t pixel = img[i];

        pixel = (pixel >> 8) | (pixel << 8);
        uint8_t r5 = (pixel >> 11) & 0x1F; // 5位红色
        uint8_t g6 = (pixel >> 5)  & 0x3F; // 6位绿色
        uint8_t b5 = pixel & 0x1F;         // 5位蓝色

        uint8_t gray = (uint8_t)(((uint16_t)r5 * 630 + (uint16_t)g6 * 608 + (uint16_t)b5 * 240) >> 8);

        // 二值化
        if(gray >= threshold)
            binary[i] = 0xFFFF; // 白色
        else
            binary[i] = 0x0000; // 黑色
    }
}

void yuyv_to_binary_self(uint16_t *buf,int width, int height, uint8_t thr);

void yuyv_to_binary(uint16_t *buf, uint16_t *binary, int width, int height, uint8_t thr) {
    int len = width * height;
    for (int i = 0; i  < len; i++) {
        uint8_t y = buf[i]&0x00FF;
        binary[i] = (y < thr) ? 0 : 0xFFFF;
    }
}

// 定义一个 128 位向量类型，包含 8 个 uint16_t
typedef uint16_t v8u16 __attribute__((vector_size(16)));

/**
 * @brief SIMD 优化的 YUYV 转二值化函数，包含重心计算和零拷贝裁剪
 *
 * 该函数使用 SIMD 指令处理输入 YUYV 图像的特定区域。
 * 它可以根据 Y 分量进行二值化，并同时计算白色像素的质心（重心）。
 * 支持通过跳过每行末尾的字节来实现零拷贝裁剪，无需额外的内存拷贝。
 *
 * @param buf        源图像缓冲区起始地址 (YUYV 格式)。
 * @param binary     输出的二值化图像缓冲区指针。如果为 NULL，则只计算重心不输出图像。
 * @param x          裁剪区域的起始 X 坐标。必须是 8 的倍数以满足 SIMD 16字节对齐要求。
 * @param y          裁剪区域的起始 Y 坐标。
 * @param width      裁剪区域的宽度。
 * @param height     裁剪区域的高度。
 * @param raw_width  源图像的完整原始宽度 (用于计算换行时的 stride 偏移)。
 * @param thr        二值化阈值 (0-255)。Y 值大于 thr 的像素被视为白色。
 * @param out_x      [输出] 计算得到的白色像素质心 X 坐标 (相对于裁剪区域左上角)。
 * @param out_y      [输出] 计算得到的白色像素质心 Y 坐标 (相对于裁剪区域左上角)。
 *
 * @return           区域内检测到的白色像素总数。如果发生对齐错误或输入无效，返回 0。
 */
uint32_t yuyv_to_binary_simd_with_cal_core(uint16_t *buf, 
                                           uint16_t* binary, 
                                           int x, 
                                           int y, 
                                           int width, 
                                           int height, 
                                           int raw_width, 
                                           uint8_t thr, 
                                           float *out_x, 
                                           float *out_y) {
    // 偏移指针到起始位置
    buf += (y * raw_width + x);

    int total_pixels = width * height;
    // 检查 buf 对齐，Binary 对齐，以及最小处理单元
    if (((uint32_t)buf & 0xF) != 0 || total_pixels < 8 || (binary && (((uint32_t)binary & 0xF) != 0))) {
        ESP_LOGW(TAG, "Input/Output buffer alignment error or too small. Fallback to scalar.");
        if (binary) {
             // 降级这里暂时不支持 stride，假设 width==raw_width, 实际应该要改但 SIMD 是重点
            yuyv_to_binary(buf, binary, width, height, thr);
        }
        *out_x = -1; *out_y = -1;
        return 0;
    }

    // 计算行尾需要跳过的字节数 (Gap)
    // 比如 原始320，处理248。读完248个像素后，指针需要往后跳 (320-248)*2 字节才能到下一行开头
    int gap_bytes = (raw_width - width) * 2;

    // 准备常量块
    uint16_t consts[32] __attribute__((aligned(16))) = {
        thr, thr, thr, thr, thr, thr, thr, thr,             // [0] q1: 阈值
        0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, 0x00FF, // [8] q2: 掩码
        1, 1, 1, 1, 1, 1, 1, 1,                             // [16] q3: 全 1 (用于 Count)
        8, 8, 8, 8, 8, 8, 8, 8                              // [24] q4: 步进 8
    };
    uint16_t x_init[16] __attribute__((aligned(16))) = {0, 1, 2, 3, 4, 5, 6, 7};
    x_init[8] = (uint16_t)gap_bytes;
    uint32_t qacc_tmp[8] __attribute__((aligned(16))); 

    uint32_t total_sum_x = 0, total_sum_y = 0, total_count = 0; 

    uint16_t *p_in = buf, *p_out = binary, *p_c = consts, *p_xi = x_init;
    uint32_t *p_res = qacc_tmp; // 指向 64 位缓冲区
    int vecs_per_row = width / 8;

    if (binary) {
        asm volatile (
            // --- 1. [初始化阶段] ---
            "ee.vld.128.ip q1, %[consts], 16 \n"    // q1 = thr
            "ee.vld.128.ip q2, %[consts], 16 \n"    // q2 = mask 0x00FF
            "ee.vld.128.ip q3, %[consts], 16 \n"    // q3 = ones [1,1...]
            "ee.vld.128.ip q4, %[consts], 0  \n"    // q4 = increment [8,8...]
            "ee.xorq q7, q7, q7 \n"          // q7 = 0
            "ee.notq q6, q7 \n"              // q6 = 0xFFFF
            
            "ee.zero.accx \n"                // 重置全局 Sum X (ACCX)
            "movi a11, 0 \n"                 // a11 = total_sum_y = 0
            "movi a12, 0 \n"                 // a12 = total_count = 0
            "movi a14, 0 \n"                 // a14 = y_index = 0
            
            "row_loop1: \n"                         // [外层 loop: 处理每一行]
            "ee.vld.128.ip q5, %[xinit], 0 \n"     // 每一行开头重置 X 序列 q5 = [0,1..7]
            "ee.zero.qacc \n"              // 重置行计数累加器 QACC
            "mov a15, %[vecs] \n"                 // a15 = 本行向量数 (width/8)
            
            "1: \n"                          // [内层 loop: 像素处理]
            "ee.vld.128.ip q0, %[in], 16 \n"     
            "ee.andq q0, q0, q2 \n"
            "ee.vsubs.s16 q0, q0, q1 \n"
            "ee.vmin.s16 q0, q0, q7 \n"
            "ee.vmax.s16 q0, q0, q6 \n"
            "ee.notq q0, q0 \n"              // q0 = 二值化 Mask
            "ee.vst.128.ip q0, %[out], 16 \n"    // 写回 binary 并递增指针
            
            "ee.andq q0, q0, q3 \n"          // mask -> [1 或 0]
            "ee.vmulas.u16.accx q0, q5 \n"   // 全局 Sum X 直接累加到 ACCX
            "ee.vmulas.u16.qacc q0, q3 \n"   // 行计数累加到 QACC
            "ee.vadds.s16 q5, q5, q4 \n"     // X 序列递增
            "addi a15, a15, -1 \n"
            "bnez a15, 1b \n"                // 内层循环结束
            
            // --- 行结束: 指针跳跃 (Gap) ---
            "l16ui a8, %[xinit], 16 \n"      // 从 x_init[8] 加载 gap_bytes
            "add %[in], %[in], a8 \n"        // 跳过行尾无效数据

            // --- 行结束处理: 统计数据 ---
            "movi a13, 0 \n"                  
            "ee.srcmb.s16.qacc q5, a13, 0 \n" // 将qacc低 16 位提取到 q5
            "ee.vst.128.ip q5, %[res], 0 \n"      // 导出 q5 到内存
            "l16ui a8, %[res], 0  \n"  "l16ui a9, %[res], 2  \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 4  \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 6  \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 8  \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 10 \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 12 \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 14 \n"  "add a8, a8, a9 \n" 

            "mull a9, a8, a14 \n"            // a9 = 行计数 * Y
            "add a11, a11, a9 \n"            // 累积全局 Sum Y
            "add a12, a12, a8 \n"            // 累积全局 Total Count
            "addi a14, a14, 1 \n"            // y_index++
            "blt a14, %[height], row_loop1 \n"       // 外层循环结束

            "ee.st.accx.ip %[res], 0 \n"         // 导出 ACCX
            "s32i a11, %[res], 8 \n"             // 导出 a11
            "s32i a12, %[res], 12 \n"            // 导出 a12

            : [in] "+r"(p_in), [out] "+r"(p_out), [consts] "+r"(p_c), [xinit] "+r"(p_xi), [res] "+r"(p_res)
            : [vecs] "r"(vecs_per_row), [height] "r"(height)
            : "a8", "a9", "a11", "a12", "a13", "a14", "a15", "memory"
        );
    } else {
        asm volatile (
            // --- 1. [初始化阶段] ---
            "ee.vld.128.ip q1, %[consts], 16 \n"    // q1 = thr
            "ee.vld.128.ip q2, %[consts], 16 \n"    // q2 = mask 0x00FF
            "ee.vld.128.ip q3, %[consts], 16 \n"    // q3 = ones [1,1...]
            "ee.vld.128.ip q4, %[consts], 0  \n"    // q4 = increment [8,8...]
            "ee.xorq q7, q7, q7 \n"          // q7 = 0
            "ee.notq q6, q7 \n"              // q6 = 0xFFFF
            
            "ee.zero.accx \n"         // 重置全局 Sum X (ACCX)
            "movi a11, 0 \n"                 // a11 = total_sum_y = 0
            "movi a12, 0 \n"                 // a12 = total_count = 0
            "movi a14, 0 \n"                 // a14 = y_index = 0
            
            "2: \n"                   // [外层 loop: 处理每一行]
            "ee.vld.128.ip q5, %[xinit], 0 \n"     // 每一行开头重置 X 序列 q5 = [0,1..7]
            "ee.zero.qacc \n"              // 重置行计数累加器 QACC
            "mov a15, %[vecs] \n"                 // a15 = 本行向量数 (width/8)
            
            "3: \n"                          // [内层 loop: 像素处理]
            "ee.vld.128.ip q0, %[in], 16 \n"     
            "ee.andq q0, q0, q2 \n"
            "ee.vsubs.s16 q0, q0, q1 \n"
            "ee.vmin.s16 q0, q0, q7 \n"
            "ee.vmax.s16 q0, q0, q6 \n"
            "ee.notq q0, q0 \n"              // q0 = 二值化 Mask
            
            "ee.andq q0, q0, q3 \n"          // mask -> [1 或 0]
            "ee.vmulas.u16.accx q0, q5 \n"   // 全局 Sum X 直接累加到 ACCX
            "ee.vmulas.u16.qacc q0, q3 \n"   // 行计数累加到 QACC
            "ee.vadds.s16 q5, q5, q4 \n"     // X 序列递增
            "addi a15, a15, -1 \n"
            "bnez a15, 3b \n"                // 内层循环结束

             // --- 行结束: 指针跳跃 (Gap) ---
            "l16ui a8, %[xinit], 16 \n"      // 从 x_init[8] 加载 gap_bytes
            "add %[in], %[in], a8 \n"        // 跳过行尾无效数据
            
            // --- 行结束处理: 统计数据 ---
            "movi a13, 0 \n"                  
            "ee.srcmb.s16.qacc q5, a13, 0 \n" // 将qacc低 16 位提取到 q5
            "ee.vst.128.ip q5, %[res], 0 \n"      // 导出 q5 到内存
            "l16ui a8, %[res], 0  \n"  "l16ui a9, %[res], 2  \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 4  \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 6  \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 8  \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 10 \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 12 \n"  "add a8, a8, a9 \n"
            "l16ui a9, %[res], 14 \n"  "add a8, a8, a9 \n" 

            "mull a9, a8, a14 \n"            // a9 = 行计数 * Y
            "add a11, a11, a9 \n"            // 累积全局 Sum Y
            "add a12, a12, a8 \n"            // 累积全局 Total Count
            "addi a14, a14, 1 \n"            // y_index++
            "blt a14, %[height], 2b \n"       // 外层循环结束

            "ee.st.accx.ip %[res], 0 \n"         // 导出 ACCX
            "s32i a11, %[res], 8 \n"             // 导出 a11
            "s32i a12, %[res], 12 \n"            // 导出 a12

            : [in] "+r"(p_in), [consts] "+r"(p_c), [xinit] "+r"(p_xi), [res] "+r"(p_res)
            : [vecs] "r"(vecs_per_row), [height] "r"(height)
            : "a8", "a9", "a11", "a12", "a13", "a14", "a15", "memory"
        );
    }
 
    uint64_t full_sum_x = ((uint64_t)qacc_tmp[1] << 32) | qacc_tmp[0];
    total_sum_x = (uint32_t)full_sum_x; 
    total_sum_y = qacc_tmp[2];
    total_count = qacc_tmp[3];

    if (total_count > 0) {
        *out_x = ((float)total_sum_x / total_count);
        *out_y = ((float)total_sum_y / total_count);
    } else {
        *out_x = -1; *out_y = -1;
    }
    return total_count;
}



void image_crop(uint16_t *src, uint16_t *dst, int src_w, int src_h, int x1, int y1, int x2, int y2) {
    if (src == NULL || dst == NULL) return;

    // 边界修正 (确保 x1,y1 在图内)
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    
    // 确保 x2,y2 限制在 [x1, width/height] 范围内
    if (x2 > src_w) x2 = src_w;
    if (y2 > src_h) y2 = src_h;

    // 左闭右开: 宽度 = x2 - x1，若 x2 <= x1 则无效
    if (x1 >= x2 || y1 >= y2) return;

    int crop_w = x2 - x1;
    int crop_h = y2 - y1;
    int copy_bytes = crop_w * sizeof(uint16_t);

    for (int i = 0; i < crop_h; i++) {
        uint16_t *src_row = src + (y1 + i) * src_w + x1;
        uint16_t *dst_row = dst + i * crop_w;
        memcpy(dst_row, src_row, copy_bytes);
    }
}

#define THRESHOLD 180

void cam_task_entry(void *par){
    
    vTaskDelay(50);
    sensor_t *s = esp_camera_sensor_get();
    if (s) {
        ESP_LOGI(TAG, "Safe configuration complete. Starting warm-up...");
        for(int i=0; i<5; i++){
            ESP_LOGI(TAG, "Fetching warm-up frame %d...", i);
            camera_fb_t *tfb = esp_camera_fb_get();
            if(tfb) {
                ESP_LOGI(TAG, "Frame %d OK (len: %zu)", i, tfb->len);
                esp_camera_fb_return(tfb);
            } else {
                ESP_LOGW(TAG, "Frame %d TIMEOUT!", i);
            }
        }
    }

    //>dump binary memory output2.bin 0x3c07ad90 0x3C0A05D0
    //>dump binary memory output1.bin 0x3c051930 0x3c07792f fb->buf
    //>dump binary memory output.bin 0x3c0a4164 0x3c0c6963 p
    while(1){
        camera_fb_t *fb = esp_camera_fb_get();
        uint8_t flag = 0;
        if (fb) {
            float cx, cy;
            // image_crop((uint16_t *)fb->buf, (uint16_t *)crop_buf, fb->width, fb->height, 0, 0,248, 240);
            
            // 直接传递 fb->buf，并告知原始宽度 fb->width，让 SIMD 函数内部跳过 padding
            // 参数: buf, binary, start_x, start_y, clip_w, clip_h, raw_w, thr, out_x, out_y
            // 注意: start_x (0) 必须是 8 的倍数
#ifdef DEBUG

            image_crop((uint16_t *)fb->buf, (uint16_t *)binary, fb->width, fb->height, 0, 0,248, 240);
            tft_display(0, 0, 248, 240, (uint16_t *)binary, 100);
#else

            if(Completion){
                yuyv_to_binary_simd_with_cal_core((uint16_t *)fb->buf, (uint16_t *)binary, 0, 0, 248, 240, fb->width, THRESHOLD, &cx, &cy);
                Completion = 0;
                xSemaphoreGive(sem_cam);
            }else{
                yuyv_to_binary_simd_with_cal_core((uint16_t *)fb->buf, NULL, 0, 0, 248, 240, fb->width, THRESHOLD, &cx, &cy);
            }
#endif
            esp_camera_fb_return(fb);
           // ESP_LOGI(TAG, "x:%.2f,y:%.2f", cx, cy);
            uint8_t buf[9];
            buf[0] = 2;
            memcpy(buf+1, &cx, 4);
            memcpy(buf+5, &cy, 4);
            xQueueSend(xMailbox, buf, 10);
            cal_frame_rate();
        } else {
            ESP_LOGW(TAG, "Failed to get frame buffer");
            vTaskDelay(10); 
        }
    }
}

void cam_task_init(void){
    esp_log_level_set(TAG, ESP_LOG_DEBUG);
    xMailbox = xQueueCreate(3, 9);
    binary = (uint8_t *)heap_caps_aligned_alloc(16, 240*320*2, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    assert(binary != NULL && "Failed to allocate memory");
    if(binary) ESP_LOGI(TAG, "Memory allocated");
    sem_cam = xSemaphoreCreateBinary();
    ov7670_init();
    ov7670_set_night_mode(0);
    ov7670_color_test(0);
    ov7670_set_clock_divider(CONFIG_CLK_DIV1);

    xTaskCreate(cam_task_entry,
                "cam_task",
                4096,
                NULL,
                21,
                NULL);
}

