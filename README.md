# PY32F002B 项目模板

目标芯片：PY32F002Bx5（Cortex-M0+，24KB Flash，3KB RAM）
开发环境：Keil MDK 5.28，ARMCC V5

---

## 目录结构

```
PY32F002B/
├── MDK-ARM/          # Keil 工程文件
├── Driver/           # Puya 驱动库
│   ├── Inc/          # 驱动头文件（hal_conf.h 在此）
│   └── Src/          # 驱动源文件
├── Components/       # 后续添加的功能代码（比如串口、LED、红外遥控等）
│   ├── Inc/
│   └── Src/
└── User/             # 主文件main（看个人习惯，一般我这里只放main文件）
```

### MDK-ARM/
Keil 工程的配置文件、启动文件、链接脚本和编译输出目录。一般不需要手动修改，通过 Keil IDE 操作即可。

### Driver/
Puya 官方提供的 HAL 驱动层，对应 STM32 生态的 HAL 库结构。

- **`Driver/Inc/py32f002b_hal_conf.h`** — 模块开关，决定哪些外设驱动被编译进来。启用一个外设只需取消对应的 `#define HAL_XXX_MODULE_ENABLED` 注释。
- `Driver/Src/py32f002b_hal_msp.c` — 外设底层初始化回调（时钟使能、GPIO 复用等），每个外设的 `HAL_XXX_MspInit()` 在此实现。
- `Driver/Src/py32f002b_it.c` — 中断服务函数入口，所有 IRQ Handler 在此注册。
- 其余 `hal_*.c` 和 `ll_*.c` 为各外设的 HAL/LL 驱动实现，按需保留。

### Components/
板级支持包，封装开发板上的具体硬件（LED、按键、调试串口）。与芯片无关的上层代码通过这里的 API 操作硬件，不直接调用 HAL。

- `BSP_LED_Init / On / Off / Toggle` — LED 控制
- `BSP_PB_Init / GetState` — 按键读取
- `BSP_USART_Config` — 调试串口初始化（依赖 `HAL_UART_MODULE_ENABLED`）

### User/
用户应用代码，是日常开发的主要工作区。

- `main.c` — 应用入口，外设初始化和主循环逻辑写在这里。
- `main.h` — 公共头文件，统一 include 入口。
- `system_py32f002b.c` — 系统时钟初始化，通常不需要修改。

---

## 添加新功能

### 1. 启用一个新外设（以 SPI 为例）

**第一步：打开模块开关**

在 `Driver/Inc/py32f002b_hal_conf.h` 中取消注释：
```c
#define HAL_SPI_MODULE_ENABLED
```

**第二步：在 Components/ 中新建驱动文件**

新建 `Components/Src/spi.c` 和 `Components/Inc/spi.h`，在其中封装初始化和使用函数：
```c
// spi.h
void SPI_Init(void);
void SPI_Transmit(uint8_t *data, uint16_t size);

// spi.c
SPI_HandleTypeDef hspi1;

void SPI_Init(void)
{
    hspi1.Instance = SPI1;
    hspi1.Init.Mode = SPI_MODE_MASTER;
    // ... 其他参数
    HAL_SPI_Init(&hspi1);
}
```

然后在 Keil 的 Components 分组中添加 `spi.c`，在 `main.h` 中 include `spi.h`。

**第三步：注册中断（如需要）**

在 `Driver/Src/py32f002b_it.c` 中添加对应的 IRQ Handler：
```c
void SPI1_IRQHandler(void)
{
    HAL_SPI_IRQHandler(&hspi1);
}
```

**第四步：在 main.c 中调用**

```c
SPI_Init();
SPI_Transmit(buf, len);
```

---

### 3. 调整时钟配置

系统时钟在 `User/system_py32f002b.c` 的 `SystemClock_Config()` 中配置，默认使用内部 HSI（24MHz）。修改分频系数或切换时钟源在此处进行。
