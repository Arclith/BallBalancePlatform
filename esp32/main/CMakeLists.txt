idf_component_register(SRCS 
"main.c" 
"drivers/my_ov7670.c" 
"drivers/drv_install.c" 
"drivers/drv_uart.c"
"drivers/tft.c"
"task/communicate_task.c"
"task/cam_task.c"
"task/lcd_task.c"
"task/shell_task.c"
INCLUDE_DIRS "." "drivers" "task"
PRIV_REQUIRES driver esp32-camera esp_lcd esp_timer )

# 开启 ESP32-S3 的向量化优化
set_source_files_properties("task/cam_task.c" PROPERTIES COMPILE_FLAGS "-O3")


