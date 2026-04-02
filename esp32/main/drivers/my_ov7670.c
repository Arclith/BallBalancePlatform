#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_camera.h"
#include "my_ov7670.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "typedef.h"

static const char *TAG = "OV7670_DRV";



#define CAM_PIN_D7      18  // 数据线 D7
#define CAM_PIN_D5      17  // 数据线 D5
#define CAM_PIN_D3      16   // 数据线 D3
#define CAM_PIN_D1      15   // 数据线 D1
#define CAM_PIN_D6      7   // 数据线 D6
#define CAM_PIN_D4      6  // 数据线 D4
#define CAM_PIN_D2      5  // 数据线 D2
#define CAM_PIN_D0      4   // 数据线 D0

#define CAM_PIN_PWDN    47  // 功耗控制 (Power Down)
#define CAM_PIN_RESET   38  // 复位引脚 (Reset)
#define CAM_PIN_XCLK    39  // 外部主时钟 (System Clock Input)
#define CAM_PIN_SIOD    41  // SCCB 数据 (I2C SDA)
#define CAM_PIN_SIOC    2   // SCCB 时钟 (I2C SCL)
#define CAM_PIN_VSYNC   1   // 场同步信号 (Vertical Sync)
#define CAM_PIN_HREF    40  // 行同步信号 (Horizontal Reference)
#define CAM_PIN_PCLK    42  // 像素时钟 (Pixel Clock)

#define CAM_CLK_FREQ_MHZ   20

static sensor_t *s = NULL;

esp_err_t ov7670_drv_install(void)
{

    camera_config_t config = {
        .pin_pwdn = CAM_PIN_PWDN,
        .pin_reset = CAM_PIN_RESET,
        .pin_xclk = CAM_PIN_XCLK,
        .pin_sccb_sda = CAM_PIN_SIOD,
        .pin_sccb_scl = CAM_PIN_SIOC,

        .pin_d7 = CAM_PIN_D7,
        .pin_d6 = CAM_PIN_D6,
        .pin_d5 = CAM_PIN_D5,
        .pin_d4 = CAM_PIN_D4,
        .pin_d3 = CAM_PIN_D3,
        .pin_d2 = CAM_PIN_D2,
        .pin_d1 = CAM_PIN_D1,
        .pin_d0 = CAM_PIN_D0,
        .pin_vsync = CAM_PIN_VSYNC,
        .pin_href = CAM_PIN_HREF,
        .pin_pclk = CAM_PIN_PCLK,

        .xclk_freq_hz = CAM_CLK_FREQ_MHZ * 1000000, 
        
        .sccb_i2c_port = -1, 
#ifdef DEBUG
        .pixel_format = PIXFORMAT_RGB565,
#else
        .pixel_format = PIXFORMAT_YUV422,
#endif
        .frame_size = FRAMESIZE_320x240,   
        .fb_count = 2,
        .fb_location = CAMERA_FB_IN_PSRAM, 
        .grab_mode = CAMERA_GRAB_WHEN_EMPTY, 
    };

    // 【核心修改 2】
    // 官方驱动在扫描时非常快，OV7670 可能还没准备好。
    // 我们手动做一次硬复位，确保时序。
    gpio_config_t rst_conf = {
        .pin_bit_mask = (1ULL << CAM_PIN_RESET) | (1ULL << CAM_PIN_PWDN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&rst_conf);
    
    gpio_set_level(CAM_PIN_PWDN, 0); // 正常模式
    gpio_set_level(CAM_PIN_RESET, 0); // 复位
    vTaskDelay(pdMS_TO_TICKS(100));     // 给够时间
    gpio_set_level(CAM_PIN_RESET, 1); // 释放复位
    vTaskDelay(pdMS_TO_TICKS(100));

    return esp_camera_init(&config);
}

void ov7670_init(void)
{

    esp_err_t ret = ov7670_drv_install();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Camera init failed with error 0x%x", ret);
        return;
    }
    s = esp_camera_sensor_get();
    if(s == NULL) {
        ESP_LOGE(TAG, "Camera sensor not found");
        return ;
    }

    if (s->set_colorbar) { s->set_colorbar(s, 0);  }
    if (s->set_whitebal) { s->set_whitebal(s, 1);  }
    if (s->set_gain_ctrl) { s->set_gain_ctrl(s, 1); }
    if (s->set_exposure_ctrl) { s->set_exposure_ctrl(s, 1); }
    if (s->set_brightness) { s->set_brightness(s, 0); }
    if (s->set_contrast) { s->set_contrast(s, 0x40);  }
    if (s->set_saturation) { s->set_saturation(s, 0x60);  }
    if (s->set_hmirror) { s->set_hmirror(s, 0);  }
    if (s->set_vflip) { s->set_vflip(s, 0);  }
    
     s->set_reg(s, 0x11, 0x3F, 0x00); 

    // 高级 Gamma 曲线校正 (0x7A-0x89)
    s->set_reg(s, 0x7A, 0xFF, 0x20); s->set_reg(s, 0x7B, 0xFF, 0x10);
    s->set_reg(s, 0x7C, 0xFF, 0x1E); s->set_reg(s, 0x7D, 0xFF, 0x35);
    s->set_reg(s, 0x7E, 0xFF, 0x5A); s->set_reg(s, 0x7F, 0xFF, 0x69);
    s->set_reg(s, 0x80, 0xFF, 0x76); s->set_reg(s, 0x81, 0xFF, 0x80);
    s->set_reg(s, 0x82, 0xFF, 0x88); s->set_reg(s, 0x83, 0xFF, 0x8F);
    s->set_reg(s, 0x84, 0xFF, 0x96); s->set_reg(s, 0x85, 0xFF, 0xA3);
    s->set_reg(s, 0x86, 0xFF, 0xAF); s->set_reg(s, 0x87, 0xFF, 0xC4);
    s->set_reg(s, 0x88, 0xFF, 0xD7); s->set_reg(s, 0x89, 0xFF, 0xE8);

    // 颜色矩阵与增益
    s->set_reg(s, 0x4F, 0xFF, 0x80); s->set_reg(s, 0x50, 0xFF, 0x80);
    s->set_reg(s, 0x51, 0xFF, 0x00); s->set_reg(s, 0x52, 0xFF, 0x22);
    s->set_reg(s, 0x53, 0xFF, 0x5E); s->set_reg(s, 0x54, 0xFF, 0x80);
    
    // 【解决过曝核心修改】
    s->set_reg(s, 0x13, 0xFF, 0xE7); // COM8: 硬件自动曝光开启
    s->set_reg(s, 0x14, 0xFF, 0x3A); // COM9: 降低自动增益上限(4x)，防止雪白一片
    s->set_reg(s, 0x24, 0xFF, 0x75); // AEW: 调低高阈值，只要稍微亮一点就强制压低快门
    s->set_reg(s, 0x25, 0xFF, 0x30); // AEB: 调低低阈值
    s->set_reg(s, 0x26, 0xFF, 0xFB); // VPT: 快速响应步长拉满，让曝光调节不再“迟钝”
   // s->set_reg(s, 0x4C, 0xFF, 0x10); 
    
    s->set_reg(s, 0x42, 0x01, 0x01); // COM17: 开启全屏曝光采样，防止局部高光导致整体过曝

}

void ov7670_set_clock_divider(uint8_t div)
{
    s->set_reg(s, 0x11, 0x3F, div);
}

void ov7670_set_night_mode(bool enable)
{
    s->set_reg(s, 0x3B, 0xFF, enable ? 0xC4 : 0x00);
}

void ov7670_color_test(bool enable)
{
     s->set_colorbar(s, enable);
}

void ov7670_framesize_set(uint8_t size)
{
    s->set_framesize(s, size);
}
