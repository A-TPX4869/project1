/**
 ******************************************************************************
 * @file    uart_hal.c
 * @brief   基于 HAL 库的 USART1 驱动实现
 *
 * 接收机制：
 *   HAL_UART_Receive_IT 每次挂起 1 字节的中断接收。
 *   每收到 1 字节触发 HAL_UART_RxCpltCallback，在回调中：
 *     1. 将字节存入 usart_rx_buf
 *     2. 若为 '\n' 则置 usart_rx_flg = 1，通知主循环处理
 *     3. 重新挂起下一次 1 字节接收
 *
 * 发送机制：
 *   hal_uart_sendbyte / hal_uart_sendstring 使用 HAL_UART_Transmit 轮询发送，
 *   调用时会阻塞直到数据全部移入发送寄存器。
 *
 * 中断入口：
 *   USART1_IRQHandler（位于 py32f002b_it.c）→ HAL_UART_IRQHandler(&huart1)
 *   → HAL_UART_RxCpltCallback（本文件）
 ******************************************************************************
 */

#include "uart_hal.h"

#if UART_ON

/* ----------------------------- 全局变量 ----------------------------------- */

/** HAL UART 句柄，贯穿所有 HAL UART API 调用 */
UART_HandleTypeDef huart1;

/** 接收缓冲区，由中断回调逐字节填充 */
u8  usart_rx_buf[MAX_REC_LENGTH] = {0};

/**
 * 接收完成标志
 * 主循环检测到该标志为 1 后应处理 usart_rx_buf，
 * 处理完毕后将其清零并重置 usart_rx_cnt
 */
u8  usart_rx_flg = 0;

/** 已接收字节计数，同时作为 usart_rx_buf 的写入下标 */
u32 usart_rx_cnt = 0;

/**
 * 单字节中转缓冲区，供 HAL_UART_Receive_IT 使用。
 * HAL 每完成 1 字节接收后将数据写入此处，
 * 回调中再将其复制到 usart_rx_buf。
 */
static u8 s_rx_byte;

/* ----------------------------- MSP 初始化 --------------------------------- */

/**
 * @brief  覆盖 HAL 弱函数，在 HAL_UART_Init 内部被自动调用
 *         负责 GPIO、外设时钟和 NVIC 的底层初始化
 * @param  huart  HAL UART 句柄指针（本实现只处理 USART1）
 */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* 开启 GPIO 和 USART1 的外设时钟 */
    UART_GPIO_CLK_EN();
    UART_CLK_EN();

    /*
     * TX 和 RX 引脚使用相同配置，一次性初始化：
     * - 推挽复用输出
     * - 上拉，防止空闲状态总线浮空
     * - 高速，满足高波特率时序要求
     */
    GPIO_InitStruct.Pin       = UART_TX_PIN | UART_RX_PIN;
    GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull      = GPIO_PULLUP;
    GPIO_InitStruct.Speed     = GPIO_SPEED_FREQ_HIGH;
    GPIO_InitStruct.Alternate = UART_GPIO_AF;
    HAL_GPIO_Init(UART_GPIO_PORT, &GPIO_InitStruct);

    /* 配置 USART1 中断优先级并使能；优先级 0（最高组），子优先级 1 */
    HAL_NVIC_SetPriority(USART1_IRQn, 0, 1);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
}

/* ----------------------------- 初始化 ------------------------------------- */

/**
 * @brief  初始化 USART1 并立即挂起首次中断接收
 * @param  baudrate  目标波特率（如 9600、115200）
 * @note   固定为 8 位数据位、1 位停止位、无校验、无硬件流控、16 倍过采样
 */
void hal_uart_init(u32 baudrate)
{
    huart1.Instance                    = USART1;
    huart1.Init.BaudRate               = baudrate;
    huart1.Init.WordLength             = UART_WORDLENGTH_8B;    /* 8 位数据位  */
    huart1.Init.StopBits               = UART_STOPBITS_1;       /* 1 位停止位  */
    huart1.Init.Parity                 = UART_PARITY_NONE;      /* 无校验      */
    huart1.Init.HwFlowCtl              = UART_HWCONTROL_NONE;   /* 无硬件流控  */
    huart1.Init.Mode                   = UART_MODE_TX_RX;       /* 全双工      */
    huart1.Init.OverSampling           = UART_OVERSAMPLING_16;  /* 16 倍过采样 */
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;

    /* 内部自动调用 HAL_UART_MspInit 完成 GPIO/时钟/NVIC 配置 */
    HAL_UART_Init(&huart1);

    /* 挂起首次 1 字节中断接收，后续由回调自动续挂 */
    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

/* ----------------------------- 发送 --------------------------------------- */

/**
 * @brief  轮询发送单个字节
 * @param  data  待发送字节
 * @note   使用 HAL_MAX_DELAY 永久等待，确保字节完整发出后才返回
 */
void hal_uart_sendbyte(u8 data)
{
    HAL_UART_Transmit(&huart1, &data, 1, HAL_MAX_DELAY);
}

/**
 * @brief  轮询发送以 '\0' 结尾的字符串
 * @param  str  字符串首地址
 */
void hal_uart_sendstring(u8 *str)
{
    while (*str)
    {
        hal_uart_sendbyte(*str++);
    }
}

/* ----------------------------- 接收回调 ----------------------------------- */

/**
 * @brief  HAL UART 接收完成回调（每收到 1 字节触发一次）
 * @param  huart  触发回调的 UART 句柄
 *
 * 处理流程：
 *   1. 过滤非 USART1 的回调
 *   2. 将 s_rx_byte 追加到 usart_rx_buf；缓冲区满时丢弃该字节
 *   3. 检测到 '\n' 表示一帧结束，置 usart_rx_flg 通知主循环
 *   4. 重新挂起下一次 1 字节接收，保持接收持续运行
 *
 * @note  主循环处理完数据后需手动执行：
 *          usart_rx_flg = 0;
 *          usart_rx_cnt = 0;
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance != USART1)
        return;

    /* 缓冲区未满时才写入，防止越界 */
    if (usart_rx_cnt < MAX_REC_LENGTH)
        usart_rx_buf[usart_rx_cnt++] = s_rx_byte;

    /* 以换行符作为帧结束标志 */
    if (s_rx_byte == '\n')
        usart_rx_flg = 1;

    /* 续挂下一次接收，保持中断接收链不中断 */
    HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
}

#endif /* UART_ON */
