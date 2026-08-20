/**
 ******************************************************************************
 * @file    uart_hal.h
 * @brief   基于 HAL 库的 USART1 驱动头文件
 *
 * 功能概述：
 *   - USART1 初始化（8N1，无硬件流控，16 倍过采样）
 *   - 轮询发送（字节 / 字符串）
 *   - 中断接收：每次接收 1 字节，存入环形缓冲区，
 *     收到 '\n' 后置完成标志 usart_rx_flg
 *
 * 引脚：PB4 = TX，PB5 = RX（复用功能 AF1）
 *
 * 典型用法：
 *   hal_uart_init(115200);            // 初始化
 *   hal_uart_sendstring((u8*)"hi\n"); // 发送
 *   if (usart_rx_flg) { ... }         // 检查接收完成
 ******************************************************************************
 */

#ifndef __UART_HAL_H
#define __UART_HAL_H

#include "py32f0xx_hal.h"
#include "base_types.h"

/* ----------------------------- 功能开关 ----------------------------------- */

/** 为 0 时整个驱动不编译，节省 Flash */
#define UART_ON          1

/* ----------------------------- 缓冲区配置 --------------------------------- */

/** 接收缓冲区最大字节数；超出部分将被丢弃 */
#define MAX_REC_LENGTH   256

/* ----------------------------- 引脚 / 时钟配置 ---------------------------- */
/* 修改串口引脚时只需改这里，无需触碰 .c 文件 */

#define UART_TX_PIN         GPIO_PIN_4                      /**< TX 引脚：PB4          */
#define UART_RX_PIN         GPIO_PIN_5                      /**< RX 引脚：PB5          */
#define UART_GPIO_PORT      GPIOB                           /**< TX/RX 所在 GPIO 端口  */
#define UART_GPIO_AF        GPIO_AF1_USART1                 /**< GPIO 复用功能编号     */
#define UART_GPIO_CLK_EN()  __HAL_RCC_GPIOB_CLK_ENABLE()    /**< 开启 GPIO 时钟  */
#define UART_CLK_EN()       __HAL_RCC_USART1_CLK_ENABLE()   /**< 开启 USART1 时钟 */

/* ----------------------------- 外部变量声明 ------------------------------- */

/** HAL UART 句柄，USART1_IRQHandler 中需要引用 */
extern UART_HandleTypeDef huart1;

/** 接收缓冲区，存放从中断逐字节收到的原始数据 */
extern u8  usart_rx_buf[MAX_REC_LENGTH];

/**
 * 接收完成标志
 *   0 = 未收到完整帧
 *   1 = 已收到 '\n'，数据就绪；处理完毕后需手动清零并重置 usart_rx_cnt
 */
extern u8  usart_rx_flg;

/** 当前已写入 usart_rx_buf 的字节数，同时作为下次写入的下标 */
extern u32 usart_rx_cnt;

/* ----------------------------- 函数声明 ----------------------------------- */

/**
 * @brief  初始化 USART1 并启动中断接收
 * @param  baudrate  波特率，例如 9600、115200
 */
void hal_uart_init(u32 baudrate);

/**
 * @brief  轮询发送单个字节，阻塞直到发送完成
 * @param  data  待发送字节
 */
void hal_uart_sendbyte(u8 data);

/**
 * @brief  轮询发送以 '\0' 结尾的字符串
 * @param  str  字符串首地址
 */
void hal_uart_sendstring(u8 *str);

#endif /* __UART_HAL_H */
