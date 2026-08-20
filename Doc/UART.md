# UART 驱动说明

## 目录

- [文件结构](#文件结构)
- [硬件配置](#硬件配置)
- [UART 参数说明](#uart-参数说明)
- [初始化流程](#初始化流程)
- [发送数据](#发送数据)
- [接收数据](#接收数据)
- [中断调用链](#中断调用链)
- [使用示例](#使用示例)
- [修改指引](#修改指引)

---

## 文件结构

```
Components/
├── Inc/uart_hal.h        # 驱动头文件：宏配置、变量声明、函数声明
└── Src/uart_hal.c        # 驱动实现：初始化、发送、中断接收回调

Driver/
└── Src/py32f002b_it.c    # 中断服务函数：USART1_IRQHandler
```

---

## 硬件配置

| 信号 | 引脚 | 复用功能 |
|------|------|----------|
| TX   | PB4  | AF1_USART1 |
| RX   | PB5  | AF1_USART1 |

GPIO 配置：推挽复用输出，上拉，高速。

---

## UART 参数说明

当前固定参数（在 `hal_uart_init` 中配置，波特率由调用方传入）：

| 参数 | 值 | 说明 |
|------|----|------|
| 数据位 | 8 bit | `UART_WORDLENGTH_8B` |
| 停止位 | 1 bit | `UART_STOPBITS_1` |
| 校验位 | 无 | `UART_PARITY_NONE` |
| 硬件流控 | 无 | `UART_HWCONTROL_NONE` |
| 模式 | 全双工 | `UART_MODE_TX_RX` |
| 过采样 | 16 倍 | `UART_OVERSAMPLING_16`，抗干扰能力强 |

> **过采样 16 vs 8**：16 倍过采样对每个 bit 采样 16 次，抗噪性更好，但要求时钟精度相对宽松；8 倍过采样可支持更高波特率，代价是噪声容限降低。通常保持 16 倍即可。

---

## 初始化流程

调用 `hal_uart_init(baudrate)` 后，内部按以下顺序执行：

```
hal_uart_init(115200)
    │
    ├─ HAL_UART_Init(&huart1)
    │       │
    │       └─ HAL_UART_MspInit(&huart1)   ← HAL 自动回调
    │               ├─ 开启 GPIOB 时钟
    │               ├─ 开启 USART1 时钟
    │               ├─ 初始化 PB4(TX) / PB5(RX) 为 AF1 复用
    │               └─ 配置并使能 USART1_IRQn 中断
    │
    └─ HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1)
            └─ 挂起首次 1 字节中断接收，启动接收链
```

**在 `main.c` 中只需一行：**

```c
hal_uart_init(115200);
```

---

## 发送数据

### 发送单字节

```c
hal_uart_sendbyte('A');
```

内部调用 `HAL_UART_Transmit`，阻塞等待直到字节完整写入发送寄存器后返回。

### 发送字符串

```c
hal_uart_sendstring((u8 *)"Hello World\r\n");
```

逐字节调用 `hal_uart_sendbyte`，遇 `'\0'` 停止。

> **注意**：发送函数为**轮询阻塞**方式。在高波特率（115200）下发送少量数据延迟可忽略；若需要发送大量数据且不希望阻塞，可改用 `HAL_UART_Transmit_IT` 或 DMA 方式。

---

## 接收数据

接收采用**中断方式**，每次接收 1 字节，以 `'\n'` 作为帧结束标志。

### 相关变量

| 变量 | 类型 | 说明 |
|------|------|------|
| `usart_rx_buf` | `u8[256]` | 接收缓冲区，存放原始字节流 |
| `usart_rx_cnt` | `u32` | 已接收字节数，同时是下次写入的下标 |
| `usart_rx_flg` | `u8` | 接收完成标志，收到 `'\n'` 后置 1 |

### 主循环处理模板

```c
extern u8  usart_rx_buf[MAX_REC_LENGTH];
extern u8  usart_rx_flg;
extern u32 usart_rx_cnt;

while (1)
{
    if (usart_rx_flg)
    {
        /* 确保字符串以 '\0' 结尾，便于字符串函数处理 */
        usart_rx_buf[usart_rx_cnt] = '\0';

        /* 处理数据，例如打印回显 */
        hal_uart_sendstring(usart_rx_buf);

        /* 清空缓冲区，准备下一帧 */
        usart_rx_cnt = 0;
        usart_rx_flg = 0;
    }
}
```

> **缓冲区满的处理**：当接收字节数达到 `MAX_REC_LENGTH`（256）后，后续字节会被丢弃，直到主循环重置 `usart_rx_cnt`。如果应用数据帧可能超过 256 字节，需要增大 `MAX_REC_LENGTH`。

---

## 中断调用链

```
硬件产生 USART1 中断
    │
    └─ USART1_IRQHandler()          [py32f002b_it.c]
            │
            └─ HAL_UART_IRQHandler(&huart1)   [HAL 库]
                    │
                    └─ HAL_UART_RxCpltCallback(&huart1)  [uart_hal.c]
                            ├─ 将 s_rx_byte 写入 usart_rx_buf[usart_rx_cnt++]
                            ├─ 若 s_rx_byte == '\n'，置 usart_rx_flg = 1
                            └─ HAL_UART_Receive_IT(...)   重新挂起接收
```

### 为什么要在 py32f002b_it.c 中添加代码

Cortex-M0+ 的中断向量表在启动文件 `startup_py32f002bxx.s` 中定义，每个外设中断对应一个固定的函数名（如 `USART1_IRQHandler`）。硬件触发中断时，CPU 直接跳转到向量表中登记的这个地址执行。

**HAL 库本身不提供这个函数的实现**，它只提供 `HAL_UART_IRQHandler` 作为中断的统一处理入口（负责判断中断来源、清除标志位、调用回调）。两者之间的连接需要开发者手动完成，也就是在 `py32f002b_it.c` 里写这几行：

```c
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
```

**为什么放在 py32f002b_it.c 而不是 uart_hal.c？**

这是 Puya/ST HAL 生态的约定：`py32f002b_it.c` 是所有中断服务函数的统一入口文件，集中管理便于排查中断冲突。`uart_hal.c` 只负责外设逻辑，不承担中断向量的注册职责。这样分工后，当项目同时用到多个外设中断时，只需在 `py32f002b_it.c` 一处查看和管理所有 IRQ Handler，不用翻阅各个驱动文件。

> 如果漏写 `USART1_IRQHandler`，启动文件里有一个同名的弱函数（`__weak`）兜底，其内容是死循环 `while(1)`。结果就是：UART 产生中断后 CPU 进入死循环，程序卡死，且不会有任何报错提示——这是中断驱动开发中常见的隐蔽 bug。

---

## 使用示例

```c
#include "uart_hal.h"

int main(void)
{
    HAL_Init();

    hal_uart_init(115200);

    hal_uart_sendstring((u8 *)"UART Ready\r\n");

    while (1)
    {
        if (usart_rx_flg)
        {
            usart_rx_buf[usart_rx_cnt] = '\0';
            hal_uart_sendstring(usart_rx_buf);  /* 回显 */
            usart_rx_cnt = 0;
            usart_rx_flg = 0;
        }
    }
}
```

---

## 修改指引

### 更换 TX/RX 引脚

只需修改 `uart_hal.h` 中的宏，`.c` 文件无需改动：

```c
/* 示例：改为 PA2(TX) / PA3(RX)，AF1 */
#define UART_TX_PIN         GPIO_PIN_2
#define UART_RX_PIN         GPIO_PIN_3
#define UART_GPIO_PORT      GPIOA
#define UART_GPIO_AF        GPIO_AF1_USART1
#define UART_GPIO_CLK_EN()  __HAL_RCC_GPIOA_CLK_ENABLE()
#define UART_CLK_EN()       __HAL_RCC_USART1_CLK_ENABLE()
```

> 修改前查阅 PY32F002B 数据手册的 **Alternate Function Mapping** 表，确认目标引脚支持该复用功能。

### 更换串口号（如改用 USART2）

需要同步修改以下几处：

**1. `uart_hal.h`** — 更新时钟使能宏和外设名称：

```c
#define UART_GPIO_AF        GPIO_AF3_USART2        /* 对应 USART2 的 AF 编号 */
#define UART_CLK_EN()       __HAL_RCC_USART2_CLK_ENABLE()
```

**2. `uart_hal.c`** — 更新句柄实例和 IRQ：

```c
/* hal_uart_init 中 */
huart1.Instance = USART2;

/* HAL_UART_MspInit 中 */
HAL_NVIC_SetPriority(USART2_IRQn, 0, 1);
HAL_NVIC_EnableIRQ(USART2_IRQn);

/* HAL_UART_RxCpltCallback 中 */
if (huart->Instance != USART2)
    return;
HAL_UART_Receive_IT(&huart1, &s_rx_byte, 1);
```

**3. `py32f002b_it.c`** — 更换中断服务函数名：

```c
void USART2_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
```

### 修改接收帧结束符

默认以 `'\n'` 作为帧结束。如需改为其他字符（如 `'\r'` 或自定义字节），修改 `uart_hal.c` 中回调函数的判断条件：

```c
/* 改为以 '\r' 结尾 */
if (s_rx_byte == '\r')
    usart_rx_flg = 1;
```

### 修改接收缓冲区大小

在 `uart_hal.h` 中修改：

```c
#define MAX_REC_LENGTH   512    /* 改为 512 字节 */
```

### 修改波特率

在调用处传入目标波特率即可，无需修改驱动文件：

```c
hal_uart_init(9600);    /* 9600 bps  */
hal_uart_init(115200);  /* 115200 bps（默认）*/
hal_uart_init(460800);  /* 高速场景  */
```
