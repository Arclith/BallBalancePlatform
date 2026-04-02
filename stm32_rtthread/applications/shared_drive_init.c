#include <stm32f1xx_hal.h>
#include "shared_drive_init.h"
#include "ring_buff.h"
#include "stm32f1xx_hal_spi.h"
#include "rtthread.h"

#define DBG_TAG "device_init"
#define DBG_LVL DBG_INFO
#include <rtdbg.h>


my_hi2c_handle_t my_hi2c1 = {
    .base = {
        .count = 0,
        .port = 1, //  I2C1
        .IT = 1,
        .DMA = 1,
    },
    .hi2c = {0},
    .i2c_sem = RT_NULL,
    .hdma_tx = {0},
    .hdma_rx = {0},
};

my_hspi_handle_t my_hspi2 = {
    .base = {
        .count = 0,
        .port = 2, 
        .IT = 1,
        .DMA = 1,
    },
    .hspi = {0},
    .hdma_tx = {0},
    .hdma_rx = {0},
};

my_huart_handle_t my_huart1 = {
    .base = {
        .count = 0,
        .port = 1, 
        .IT = 1,
        .DMA = 1,
        .rb = {0},
    },
    .huart = {0},
    .hdma_tx = {0},
    .hdma_rx = {0},
};

my_huart_handle_t my_huart3 = {
    .base = {
        .count = 0,
        .port = 3, 
        .IT = 1,
        .DMA = 1,
        .rb = {0},
        .rx_sem = RT_NULL,
    },
    .huart = {0},
    .hdma_tx = {0},
    .hdma_rx = {0},
    .rb_rx = {0},
};

rb_t rb_i2c1;
rb_t rb_spi2;

uint8_t i2c1_buf[RB_I2C1_SIZE];
uint8_t spi2_buf[RB_SPI2_SIZE];
uint8_t uart1_buf[RB_UART1_SIZE];
uint8_t uart3_buf[RB_UART3_SIZE];
uint8_t uart3_rx_buf[RB_UART3_RX_SIZE];

void rb_i2c1_handler(rb_t *rb){
    if(rb->read == rb->write){
        rt_sem_release(my_hi2c1.i2c_sem);
        rb->isfree=1;
        return;
    }else{
        rb->isfree=0;
        frame_head_t frame_head;
        static uint8_t temp_buf[I2C1_READ_TEMP_SIZE];
        int8_t ret =rb_read(rb, temp_buf, I2C1_READ_TEMP_SIZE, &frame_head);
        LOG_D("Len:%d read:%d write:%d\n",frame_head.data_len,rb->read,rb->write);
        if(ret < 0){
            LOG_E("RB_i2c Read Error! FrameLen:%d, BufSize:%d", 
                        frame_head.data_len, I2C1_READ_TEMP_SIZE);
             return;
        }
        if(frame_head.device == oled){
            if(frame_head.mode == IT){
                while(HAL_I2C_Master_Transmit_IT(&my_hi2c1.hi2c, 0x78, temp_buf, frame_head.data_len) != HAL_OK){
                    LOG_D("OLED IT transmit error");
                }
            }
            if(frame_head.mode == DMA) {
                while(HAL_I2C_Master_Transmit_DMA(&my_hi2c1.hi2c, 0x78, temp_buf, frame_head.data_len) != HAL_OK){
                    LOG_D("OLED DMA transmit error");
                }
            }
        }
        else if(frame_head.device == mpu){
            if(frame_head.mode == IT) {
                while(HAL_I2C_Master_Transmit_IT(&my_hi2c1.hi2c, 0x68<<1, temp_buf, frame_head.data_len) != HAL_OK){
                    LOG_D("MPU IT transmit error");
                }
            }
            if(frame_head.mode == DMA) {
                while(HAL_I2C_Master_Transmit_DMA(&my_hi2c1.hi2c, 0x68<<1, temp_buf, frame_head.data_len) != HAL_OK){
                    LOG_D("MPU DMA transmit error");
                }
            }
        }
    
    }
}

void i2c_init(my_hi2c_handle_t *my_hi2c){
    
    //引用计数为零,初始化外设
    if(my_hi2c->base.count == 0){

        //初始化i2c1
        if(my_hi2c->base.port == 1){

            rb_init(&rb_i2c1,i2c1_buf,RB_I2C1_SIZE,rb_i2c1_handler);
            my_hi2c->i2c_sem = rt_sem_create("i2c1_mutex",1,RT_IPC_FLAG_FIFO);

            my_hi2c->hi2c.Instance = I2C1;
            my_hi2c->hi2c.Init.ClockSpeed = 100000;                
            my_hi2c->hi2c.Init.DutyCycle = I2C_DUTYCYCLE_2;
            my_hi2c->hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
            if (HAL_I2C_Init(&my_hi2c->hi2c) != HAL_OK){
                LOG_E("I2C1 init error");
                return;
            }

            if(my_hi2c->base.DMA){
                __HAL_RCC_DMA1_CLK_ENABLE(); // 使能 DMA1 时钟
                
                /* 配置 I2C1_TX 对应的 DMA 通道 (DMA1 Channel 6) */
                my_hi2c->hdma_tx.Instance = DMA1_Channel6;
                my_hi2c->hdma_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;      // 传输方向：内存 -> 外设
                my_hi2c->hdma_tx.Init.PeriphInc = DMA_PINC_DISABLE;         // 外设地址不增（固定为 I2C DR 寄存器）
                my_hi2c->hdma_tx.Init.MemInc = DMA_MINC_ENABLE;             // 内存地址自增（读取数组）
                my_hi2c->hdma_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE; // 外设数据宽度：8位
                my_hi2c->hdma_tx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;    // 内存数据宽度：8位
                my_hi2c->hdma_tx.Init.Mode = DMA_NORMAL;                    // 普通模式（发完一次即停止）
                my_hi2c->hdma_tx.Init.Priority = DMA_PRIORITY_LOW;          // 优先级：低
                
                HAL_DMA_Init(&my_hi2c->hdma_tx);
                /* Enable and set DMA TX IRQ (required for HAL DMA completion callbacks) */
                HAL_NVIC_SetPriority(DMA1_Channel6_IRQn, 5, 0);
                HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
                /* 将 DMA 句柄与 I2C 句柄关联 */
                __HAL_LINKDMA(&my_hi2c->hi2c, hdmatx, my_hi2c->hdma_tx);

                /* 配置 I2C1_RX 对应的 DMA 通道 (DMA1 Channel 7) */
                my_hi2c->hdma_rx.Instance = DMA1_Channel7;
                my_hi2c->hdma_rx.Init.Direction = DMA_PERIPH_TO_MEMORY;      // 传输方向：外设 -> 内存
                my_hi2c->hdma_rx.Init.PeriphInc = DMA_PINC_DISABLE;         // 外设地址不增（固定为 I2C DR 寄存器）
                my_hi2c->hdma_rx.Init.MemInc = DMA_MINC_ENABLE;             // 内存地址自增（写入数组）
                my_hi2c->hdma_rx.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE; // 外设数据宽度：8位
                my_hi2c->hdma_rx.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;    // 内存数据宽度：8位
                my_hi2c->hdma_rx.Init.Mode = DMA_NORMAL;                    // 普通模式（收完一次即停止）
                my_hi2c->hdma_rx.Init.Priority = DMA_PRIORITY_LOW;          // 优先级：低
                HAL_DMA_Init(&my_hi2c->hdma_rx);

                /* Enable and set DMA RX IRQ as well */
                HAL_NVIC_SetPriority(DMA1_Channel7_IRQn, 5, 0);
                HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);

                /* 将 DMA RX 句柄与 I2C 句柄关联 */
                __HAL_LINKDMA(&my_hi2c->hi2c, hdmarx, my_hi2c->hdma_rx);

            }

        //初始化i2c2
        }else{
            my_hi2c->hi2c.Instance = I2C2;
            my_hi2c->hi2c.Init.ClockSpeed = 100000;                
            my_hi2c->hi2c.Init.DutyCycle = I2C_DUTYCYCLE_2;
            my_hi2c->hi2c.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
            if (HAL_I2C_Init(&my_hi2c->hi2c) != HAL_OK){
                LOG_E("I2C2 init error");
                return;
            }
        }
    }
    my_hi2c->base.count++;
}


//处理I2C传输完成回调
void HAL_I2C_MasterTxCpltCallback(I2C_HandleTypeDef *hi2c){
    // 【核心修复】：由于 F1 的 DMA 搬运极快，可能在最后一个字节发完前就触发了 DMA TC (传输完成)中断。
    // 在 DMA TC 回调中，HAL 库会直接关闭 `I2C_IT_ERR`！
    // 导致总线上发生 NACK 时，ERR 中断被屏蔽，从而永远不会走到 I2C1_ER_IRQHandler，而是假装“发送完成”。
    // 我们必须在这里强行打补丁，检查 AF（NACK）标志位：
    if(__HAL_I2C_GET_FLAG(hi2c, I2C_FLAG_AF)) {
        __HAL_I2C_CLEAR_FLAG(hi2c, I2C_FLAG_AF); // 清除标志
        hi2c->ErrorCode |= HAL_I2C_ERROR_AF;     // 标记错误状态
        HAL_I2C_ErrorCallback(hi2c);             // 手动跳转去执行错误回调！
        return;
    }

    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
    rb_ITcallback(&rb_i2c1);
    HAL_GPIO_TogglePin(GPIOB, GPIO_PIN_12);
}

extern struct rt_semaphore mpu_sem;
void HAL_I2C_ErrorCallback(I2C_HandleTypeDef *hi2c)
{
    LOG_E("I2C Error: 0x%lx", hi2c->ErrorCode);

    // 1. 强制关闭相关中断
    __HAL_I2C_DISABLE_IT(hi2c, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR);

    // 2. 中止可能未完成的 DMA 传输
    if(hi2c->hdmatx != NULL) {
        HAL_DMA_Abort(hi2c->hdmatx);
    }
    if(hi2c->hdmarx != NULL) {
        HAL_DMA_Abort(hi2c->hdmarx);
    }

    // 3. 解除总线外设层面的所有占用和挂起标志
    __HAL_UNLOCK(hi2c);

    // 4. 使用反初始化完整清除状态 (它内部也会执行软件复位)
    HAL_I2C_DeInit(hi2c);

    // 5. 也可以保留手动 SWRST 以防止挂死，这是F1的传统老方子
    hi2c->Instance->CR1 |= I2C_CR1_SWRST;
    hi2c->Instance->CR1 &= ~I2C_CR1_SWRST;

    // 6. 重置状态标记
    hi2c->State = HAL_I2C_STATE_RESET; 
    
    // 7. 重新初始化
    HAL_I2C_Init(hi2c);

    if(hi2c->Instance == I2C1) {
        rt_sem_release(&mpu_sem);
    }
}

void rb_spi2_handler(rb_t *rb){
    if(rb->read == rb->write){
        rb->isfree=1;
        return;
    }else{
        rb->isfree=0;
        frame_head_t frame_head;
        static uint8_t temp_buf[SPI2_READ_TEMP_SIZE];
        int8_t ret =rb_read(rb, temp_buf, SPI2_READ_TEMP_SIZE, &frame_head);
      //  rt_kprintf("Len:%d read:%d write:%d\n",frame_head.data_len,rb->read,rb->write);
        if(ret < 0){
            LOG_E("RB_spi Read Error! FrameLen:%d, BufSize:%d", 
                        frame_head.data_len, SPI2_READ_TEMP_SIZE);
             return;
        }
        if(frame_head.device == w25q){
            if(frame_head.mode == IT){
                while(HAL_SPI_Transmit_IT(&my_hspi2.hspi, temp_buf, frame_head.data_len) != HAL_OK){
                    LOG_D("w25q IT transmit error");
                }
            }
            if(frame_head.mode == DMA) {
                while(HAL_SPI_Transmit_DMA(&my_hspi2.hspi, temp_buf, frame_head.data_len) != HAL_OK){
                    LOG_D("w25q DMA transmit error");
                }
            }
        }
    
    }
}



void spi_init(my_hspi_handle_t *my_hspi){
    if(my_hspi->base.count == 0){

        //初始化spi2
        if(my_hspi->base.port == 2){

            rb_init(&rb_spi2,spi2_buf,RB_SPI2_SIZE,rb_spi2_handler);

            my_hspi->hspi.Instance = SPI2;  // SPI2
            my_hspi->hspi.Init.Mode = SPI_MODE_MASTER;//主模式
            my_hspi->hspi.Init.Direction = SPI_DIRECTION_2LINES;//全双工
            my_hspi->hspi.Init.DataSize = SPI_DATASIZE_8BIT;//8位数据帧
            my_hspi->hspi.Init.CLKPolarity = SPI_POLARITY_LOW;//时钟空闲低电平
            my_hspi->hspi.Init.CLKPhase = SPI_PHASE_1EDGE;//数据捕获于第1个时钟沿
            my_hspi->hspi.Init.NSS = SPI_NSS_SOFT;//软件管理NSS信号
            my_hspi->hspi.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_16;//波特率预分频16
            my_hspi->hspi.Init.FirstBit = SPI_FIRSTBIT_MSB;//高位先传输
            my_hspi->hspi.Init.CRCPolynomial = 10;//
            if (HAL_SPI_Init(&my_hspi->hspi) != HAL_OK){
                LOG_E("SPI2 init error");
                return;
            }
        }
    }
    my_hspi->base.count++;
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi){
    rb_ITcallback(&rb_spi2);
}

extern uint8_t tx_flag;
void rb_uart1_handler(rb_t *rb){

    if(rb->read == rb->write){
        rb->isfree=1;
        return;
    }else{
        rb->isfree=0;
        static uint8_t temp_buf[UART1_READ_TEMP_SIZE];
        frame_head_t frame_head;
        int8_t ret = rb_read(rb,temp_buf,UART1_READ_TEMP_SIZE,&frame_head);
        if(ret < 0){
            LOG_E("RB_spi Read Error! FrameLen:%d, BufSize:%d", 
                        frame_head.data_len, UART1_READ_TEMP_SIZE);
             return;
        }
        if(frame_head.mode == IT){
            while(HAL_UART_Transmit_IT(&my_huart1.huart, temp_buf, frame_head.data_len) != HAL_OK){
                LOG_D("uart1 IT transmit error");
            }
        }
        if(frame_head.mode == DMA) {
            while(HAL_UART_Transmit_DMA(&my_huart1.huart, temp_buf, frame_head.data_len) != HAL_OK){
                LOG_D("uart1 DMA transmit error");
        
            }    
        }
    }
}

void rb_uart3_handler(rb_t *rb){

    if(rb->read == rb->write){
        rb->isfree=1;
        return;
    }else{
        rb->isfree=0;
        static uint8_t temp_buf[UART3_READ_TEMP_SIZE];
        frame_head_t frame_head;
        int8_t ret = rb_read(rb,temp_buf,UART3_READ_TEMP_SIZE,&frame_head);
        if(ret < 0){
            LOG_E("RB_spi Read Error! FrameLen:%d, BufSize:%d", 
                        frame_head.data_len, UART3_READ_TEMP_SIZE);
             return;
        }
        if(frame_head.mode == IT){
            while(HAL_UART_Transmit_IT(&my_huart3.huart, temp_buf, frame_head.data_len) != HAL_OK){
                LOG_D("uart1 IT transmit error");
            }
        }
        if(frame_head.mode == DMA) {
            while(HAL_UART_Transmit_DMA(&my_huart3.huart, temp_buf, frame_head.data_len) != HAL_OK){
                LOG_D("uart1 DMA transmit error");
        
            }    
        }
    }
}

uint8_t rx_buf;

void uart_init(my_huart_handle_t *my_huart){
    if(my_huart->base.count == 0){

        //初始化uart1
        if(my_huart->base.port == 1){

            rb_init(&my_huart->base.rb,uart1_buf,RB_UART1_SIZE,rb_uart1_handler);

            //uart1已经被rtthread初始化,直接使用
            my_huart->huart.Instance = USART1;
            my_huart->huart.gState = HAL_UART_STATE_READY;
            my_huart->huart.RxState = HAL_UART_STATE_READY;
        }
        if(my_huart->base.port == 3){

            __HAL_RCC_GPIOB_CLK_ENABLE();
            GPIO_InitTypeDef GPIO_InitStruct = {0};
            GPIO_InitStruct.Pin = GPIO_PIN_10;
            GPIO_InitStruct.Mode = GPIO_MODE_AF_PP; 
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; 
            GPIO_InitStruct.Pull = GPIO_PULLUP;
            HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

            GPIO_InitStruct.Pin = GPIO_PIN_11; 
            GPIO_InitStruct.Mode = GPIO_MODE_AF_INPUT; 
            GPIO_InitStruct.Pull = GPIO_PULLUP;
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH; 
            HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

            rb_init(&my_huart->base.rb,uart3_buf,RB_UART3_SIZE,rb_uart3_handler);
            rb_rx_init(&my_huart->rb_rx,uart3_rx_buf,RB_UART3_RX_SIZE);
            my_huart->base.rx_sem = rt_sem_create("uart3_rx_sem",0,RT_IPC_FLAG_FIFO);

            my_huart->huart.Instance = USART3;
            my_huart->huart.Init.BaudRate = 115200;
            my_huart->huart.Init.WordLength = UART_WORDLENGTH_8B;
            my_huart->huart.Init.StopBits = UART_STOPBITS_1;
            my_huart->huart.Init.Parity = UART_PARITY_NONE;
            my_huart->huart.Init.Mode = UART_MODE_TX_RX;
            my_huart->huart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
            my_huart->huart.Init.OverSampling = UART_OVERSAMPLING_16;

            __HAL_RCC_USART3_CLK_ENABLE();
            HAL_NVIC_SetPriority(USART3_IRQn, 1, 0);
            HAL_NVIC_EnableIRQ(USART3_IRQn);
            if (HAL_UART_Init(&my_huart->huart) != HAL_OK){
                LOG_E("UART3 init error");
                return;
            }
        HAL_UART_Receive_IT(&my_huart->huart, &rx_buf, 1);
        }
    }
    my_huart->base.count++;
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart){
    if(huart->Instance == USART3){
        rb_ITcallback(&my_huart3.base.rb);
    }
    if(huart->Instance == USART1){
        rb_ITcallback(&my_huart1.base.rb);
    }
}