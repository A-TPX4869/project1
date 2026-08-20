# PY32F002B 项目模板

目标芯片：PY32F002Bx5（Cortex-M0+，24KB Flash，3KB RAM）  
开发环境：Keil MDK 5，ARMCC V5

---

## 目录结构

```
project1/
├── Doc/                          # 功能模块详细文档
│   └── UART.md                   # UART 配置与使用说明
│
├── Driver/                       # Puya 官方 HAL/LL 驱动库（不建议修改）
│   ├── Inc/                      # 驱动头文件
│   │   ├── py32f002b_hal_conf.h  # 外设模块开关
│   │   ├── py32f002b_it.h        # 中断函数声明
│   │   ├── py32f002b_hal_*.h     # 各外设 HAL 驱动头文件
│   │   └── py32f002b_ll_*.h      # 各外设 LL 驱动头文件
│   └── Src/                      # 驱动源文件
│       ├── py32f002b_hal_msp.c   # 外设底层初始化回调（时钟、GPIO 复用）
│       ├── py32f002b_it.c        # 中断服务函数入口
│       ├── py32f002b_hal_*.c     # 各外设 HAL 实现
│       └── py32f002b_ll_*.c      # 各外设 LL 实现
│
├── Components/                   # 板级驱动（自行编写的功能模块）
│   ├── base_types.h              # 基础类型定义（u8/u16/u32 等别名）
│   ├── Inc/
│   │   ├── py32f002bxx_Start_Kit.h  # 板载 LED / 按键 BSP
│   │   └── uart_hal.h               # UART 驱动头文件
│   └── Src/
│       ├── py32f002bxx_Start_Kit.c  # 板载 LED / 按键实现
│       └── uart_hal.c               # UART 驱动实现
│
├── User/                         # 用户应用代码（日常开发的主要工作区）
│   ├── main.h                    # 公共头文件
│   ├── main.c                    # 应用入口、主循环
│   └── system_py32f002b.c        # 系统时钟初始化
│
└── MDK-ARM/                      # Keil 工程文件
    ├── PY32F002B.uvprojx         # 工程文件（用 Keil 打开）
    ├── PY32F002B.uvoptx          # 调试/下载配置
    └── startup_py32f002bxx.s     # 启动文件
```

---

## 快速上手

### 1. 打开工程

用 Keil MDK 打开 `MDK-ARM/PY32F002B.uvprojx`，直接编译下载即可运行默认示例（LED 闪烁）。

### 2. 启用 UART

在 `main.c` 中添加：

```c
#include "uart_hal.h"

int main(void)
{
    HAL_Init();

    hal_uart_init(115200);                          /* 初始化 UART，PB4=TX，PB5=RX */
    hal_uart_sendstring((u8 *)"Hello PY32!\r\n");   /* 发送字符串 */

    while (1)
    {
        if (usart_rx_flg)                           /* 收到完整一行（以 '\n' 结尾） */
        {
            usart_rx_buf[usart_rx_cnt] = '\0';
            hal_uart_sendstring(usart_rx_buf);      /* 回显 */
            usart_rx_cnt = 0;
            usart_rx_flg = 0;
        }
    }
}
```

> UART 详细说明见 [Doc/UART.md](Doc/UART.md)。

### 3. 启用新外设

在 `Driver/Inc/py32f002b_hal_conf.h` 中取消对应模块的注释：

```c
#define HAL_SPI_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
/* ... */
```

然后在 `Components/` 中新建对应的驱动文件，参考下方[添加新功能](#添加新功能)章节。

---

## 当前功能模块

### LED / 按键（py32f002bxx_Start_Kit）

板载资源 BSP，提供 LED 和按键的简单控制接口。

```c
BSP_LED_Init(LED_GREEN);       /* 初始化 LED */
BSP_LED_On(LED_GREEN);         /* 点亮 */
BSP_LED_Off(LED_GREEN);        /* 熄灭 */
BSP_LED_Toggle(LED_GREEN);     /* 翻转 */

BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);   /* 初始化按键 */
BSP_PB_GetState(BUTTON_USER);                 /* 读取按键状态 */
```

### UART（uart_hal）

基于 HAL 库的 USART1 驱动，8N1，轮询发送 + 中断接收。

| 引脚 | 功能 |
|------|------|
| PB4  | TX   |
| PB5  | RX   |

```c
hal_uart_init(115200);              /* 初始化 */
hal_uart_sendbyte(0xAA);            /* 发送单字节 */
hal_uart_sendstring((u8 *)"hi\n"); /* 发送字符串 */
/* 接收：中断自动填充 usart_rx_buf，usart_rx_flg 置 1 后处理 */
```

> 详见 [Doc/UART.md](Doc/UART.md)。

---

## 驱动层说明

### Driver/Inc/py32f002b_hal_conf.h — 外设模块开关

控制哪些 HAL 外设驱动被编译进来。默认只开启了必要模块，按需取消注释：

```c
#define HAL_GPIO_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
/* #define HAL_SPI_MODULE_ENABLED */
/* #define HAL_I2C_MODULE_ENABLED */
```

### Driver/Src/py32f002b_hal_msp.c — 底层硬件初始化

每个外设的 `HAL_XXX_MspInit()` 在此实现（时钟使能、GPIO 复用配置）。  
**例外**：`uart_hal.c` 中直接覆盖了 `HAL_UART_MspInit` 弱函数，UART 的底层配置自包含在驱动文件里，无需在此修改。

### Driver/Src/py32f002b_it.c — 中断服务函数

所有外设的 IRQ Handler 在此注册，调用对应的 `HAL_XXX_IRQHandler`：

```c
void USART1_IRQHandler(void)
{
    HAL_UART_IRQHandler(&huart1);
}
```

### Components/base_types.h — 基础类型别名

提供 `u8 / u16 / u32 / s8 / s16 / s32` 等简短类型别名，在所有 Components 文件中通用。

---

## 添加新功能

以新增 SPI 驱动为例：

**第一步**：在 `Driver/Inc/py32f002b_hal_conf.h` 打开模块开关

```c
#define HAL_SPI_MODULE_ENABLED
```

**第二步**：在 `Components/` 新建驱动文件

```c
/* Components/Inc/spi_hal.h */
void spi_init(void);
void spi_transmit(uint8_t *buf, uint16_t len);

/* Components/Src/spi_hal.c */
SPI_HandleTypeDef hspi1;

void HAL_SPI_MspInit(SPI_HandleTypeDef *hspi) { /* GPIO / 时钟配置 */ }

void spi_init(void)
{
    hspi1.Instance = SPI1;
    /* ... */
    HAL_SPI_Init(&hspi1);
}
```

**第三步**：在 `Driver/Src/py32f002b_it.c` 注册中断（如需要）

```c
#include "spi_hal.h"

void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi1);
}
```

**第四步**：在 Keil 工程的 Components 分组中添加 `spi_hal.c`，然后在 `main.h` 中 include `spi_hal.h`。

---

## 调整系统时钟

系统时钟在 `User/system_py32f002b.c` 中配置，默认使用内部 HSI（24MHz）。修改分频系数或切换时钟源在此处进行。

---

## 文档索引

| 文档 | 内容 |
|------|------|
| [Doc/UART.md](Doc/UART.md) | UART 参数详解、接收发送流程、引脚/串口号修改指引 |
