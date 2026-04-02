#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "sdkconfig.h"

static const char *TAG = "shell_task";

/**
 * @brief 辅助函数：将内核统计字符串中的 Tab 替换为垂直对齐的空格，并实现截断
 */
static void print_aligned_info(char *buf, int max_cols) {
    char *line = strtok(buf, "\n");
    while (line != NULL) {
        int col = 0;
        char *p = line;
        char *token = NULL;
        char *saveptr;

        // 手动解析列，确保前几列对齐
        token = strtok_r(p, "\t ", &saveptr);
        while (token != NULL && col < max_cols) {
            if (col == 0) printf("%-16s", token);      // Name
            else if (col == 1) printf("%-8s", token);  // State / Time
            else if (col == 2) printf("%-8s", token);  // Prio / %
            else if (col == 3) printf("%-12s", token); // Stack
            else if (col == 4) printf("%-4s", token);  // Num
            
            token = strtok_r(NULL, "\t ", &saveptr);
            col++;
        }
        printf("\n");
        line = strtok(NULL, "\n");
    }
}

/**
 * @brief 打印任务运行状态（CPU利用率）
 * 注意：需要在 sdkconfig 中开启：
 * CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS=y
 * CONFIG_FREERTOS_STATS_FORMATTING_FUNCTIONS=y
 */
void shell_cmd_stats(void) {
#if CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    char *buf = malloc(2048);
    if (buf) {
        vTaskGetRunTimeStats(buf);
        printf("\n%-16s %-15s %-10s\n", "Name", "Abs Time", "%");
        printf("-------------------------------------------\n");
        // Stats 只有3列信息
        char *line = strtok(buf, "\n");
        while (line != NULL) {
            char name[16], time[20], perc[10];
            if (sscanf(line, "%s %s %s", name, time, perc) == 3) {
                printf("%-16s %-15s %-10s\n", name, time, perc);
            }
            line = strtok(NULL, "\n");
        }
        free(buf);
    }
#else
    printf("Error: CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS disabled\n");
#endif
}

/**
 * @brief Print task list (stack, priority, core ID)
 * Requires:
 * CONFIG_FREERTOS_USE_TRACE_FACILITY=y
 * CONFIG_FREERTOS_STATS_FORMATTING_FUNCTIONS=y
 * CONFIG_FREERTOS_VTASKLIST_INCLUDE_COREID=y
 */
void shell_cmd_tasks(void) {
#if CONFIG_FREERTOS_USE_TRACE_FACILITY
    char *buf = malloc(2048);
    if (buf) {
        vTaskList(buf);
        printf("\n%-16s %-8s %-8s %-12s %-4s\n", "Name", "State", "Prio", "Stack(Min)", "Num");
        printf("----------------------------------------------------------\n");
        // 使用手动解析确保对齐并去掉最后一列(Core)
        char *line = strtok(buf, "\n");
        while (line != NULL) {
            char name[16], state[5], prio[5], stack[12], num[5];
            // 只取前5列
            if (sscanf(line, "%s %s %s %s %s", name, state, prio, stack, num) >= 5) {
                printf("%-16s %-8s %-8s %-12s %-4s\n", name, state, prio, stack, num);
            }
            line = strtok(NULL, "\n");
        }
        free(buf);
    }
#else
    printf("Error: CONFIG_FREERTOS_USE_TRACE_FACILITY disabled\n");
#endif
}

/**
 * @brief Print heap memory info
 */
void shell_cmd_heap(void) {
    multi_heap_info_t info;
    printf("\n--- Heap Summary ---\n");
    printf("%-20s: %u bytes\n", "Free Heap", (unsigned)esp_get_free_heap_size());
    printf("%-20s: %u bytes\n", "Min Free ever", (unsigned)esp_get_minimum_free_heap_size());
    
    heap_caps_get_info(&info, MALLOC_CAP_INTERNAL);
    printf("%-20s: Free: %u, Max Block: %u\n", "Internal RAM", info.total_free_bytes, info.largest_free_block);
    
    heap_caps_get_info(&info, MALLOC_CAP_SPIRAM);
    printf("%-20s: Free: %u, Max Block: %u\n", "External PSRAM", info.total_free_bytes, info.largest_free_block);
}

static void shell_task(void *pvParameters) {
    char line[64];
    int idx = 0;

    printf("\n[Monitor Shell] Type 'help' to start\n");
    printf("esp32> ");
    fflush(stdout);

    while (1) {
        int c = fgetc(stdin);

        // USB-JTAG 没输入时常返回 -1 (EOF) 或 0x0
        if (c == EOF || c == 0) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        // 处理回车
        if (c == '\r' || c == '\n') {
            printf("\n");
            line[idx] = '\0';
            
            if (idx > 0) {
                if (strcmp(line, "stats") == 0) shell_cmd_stats();
                else if (strcmp(line, "tasks") == 0) shell_cmd_tasks();
                else if (strcmp(line, "heap") == 0) shell_cmd_heap();
                else if (strcmp(line, "help") == 0) {
                    printf("stats - CPU Utilization\ntasks - Thread Stack/State\nheap  - Memory info\n");
                } else {
                    printf("Unknown: %s\n", line);
                }
            }
            
            idx = 0;
            printf("esp32> ");
            fflush(stdout);
        } 
        // 处理退格 (0x08 或 0x7F)
        else if (c == 0x08 || c == 0x7F) {
            if (idx > 0) {
                idx--;
                printf("\b \b");
                fflush(stdout);
            }
        }
        // 普通可见字符格式化
        else if (c >= 32 && c <= 126) {
            if (idx < sizeof(line) - 1) {
                line[idx++] = (char)c;
                fputc(c, stdout);
                fflush(stdout);
            }
        }
    }
}

void shell_task_init(void) {
    // 优先级设低一点，不要影响核心业务
    xTaskCreate(shell_task, "shell_ui", 4096, NULL, 5, NULL);
}
