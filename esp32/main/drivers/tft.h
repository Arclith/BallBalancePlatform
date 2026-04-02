#ifndef TFT_H
#define TFT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"

esp_err_t tft_init(void);
esp_err_t tft_send_cmd(int cmd, const void *param, size_t param_size);
esp_err_t tft_display(int x0, int y0, int x1, int y1, const void *bitmap,uint32_t timeout_ms);
void tft_clear(void);

#ifdef __cplusplus
}
#endif

#endif /* TFT_H */