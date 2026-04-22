# ESP-Claw 在 Unihiker K10 上的完整部署指南

> 本文档记录了从 ESP-IDF 安装到在 DFRobot Unihiker K10（行空板 K10）上成功运行 ESP-Claw 的全部步骤，确保可复现。

---

## 一、环境准备

### 1.1 硬件
- **Unihiker K10**（ESP32-S3 N16R8，16MB Flash + 8MB PSRAM）
- USB Type-C 数据线（连接 K10 靠近 Reset 键的 USB 口）
- macOS / Linux / Windows（WSL2）开发环境

### 1.2 安装 ESP-IDF v5.5.4（命令行方式）

**不要使用 EIM（ESP-IDF Installation Manager）**，因为 v5.5.4 存在子模块凭据 Bug。

```bash
mkdir -p ~/esp
cd ~/esp

# 克隆 ESP-IDF（使用 GitHub 源，避免 jihulab 凭据问题）
git clone -b v5.5 --recursive https://github.com/espressif/esp-idf.git

# 安装工具链
cd ~/esp/esp-idf
./install.sh esp32s3

# 导出环境变量（每次新终端都需要执行）
. ./export.sh
```

验证安装：
```bash
idf.py --version
# 应输出 ESP-IDF v5.5.4 或类似版本
```

---

## 二、克隆 ESP-Claw 仓库

```bash
cd ~/github  # 或你的工作目录
git clone --recursive https://github.com/espressif/esp-claw.git
```

---

## 三、项目初始配置

### 3.1 进入 basic_demo 项目

```bash
cd ~/github/esp-claw/application/basic_demo
```

### 3.2 安装依赖组件

```bash
# 安装 ESP Board Manager（必须）
idf.py add-dependency espressif/esp_board_manager

# 安装 LCD 驱动（必须）
idf.py add-dependency espressif/esp_lcd_ili9341

# 安装触摸屏驱动（解决 lua_module_lcd_touch 依赖）
idf.py add-dependency espressif/esp_lcd_touch
```

### 3.3 修复项目 CMakeLists.txt（添加组件搜索路径）

ESP-Claw 的组件分布在嵌套目录中，需要添加 `EXTRA_COMPONENT_DIRS`。

编辑 `CMakeLists.txt`：

```cmake
cmake_minimum_required(VERSION 3.16)

include("${CMAKE_SOURCE_DIR}/tools/cmake/esp_idf_patch.cmake")
include("${CMAKE_SOURCE_DIR}/tools/cmake/flash_partition_defaults.cmake")

# 关键：添加 ESP-Claw 子组件目录
set(EXTRA_COMPONENT_DIRS 
    "${CMAKE_SOURCE_DIR}/../../components/claw_capabilities"
    "${CMAKE_SOURCE_DIR}/../../components/claw_modules"
    "${CMAKE_SOURCE_DIR}/../../components/lua_modules"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(basic_demo)
include("${CMAKE_SOURCE_DIR}/tools/cmake/board_manager_patch.cmake")

include("${CMAKE_SOURCE_DIR}/tools/cmake/skills_sync.cmake")

fatfs_create_spiflash_image(storage fatfs_image FLASH_IN_PROJECT)
basic_demo_enable_component_skills_sync(
    TARGET fatfs_storage_bin
    DEMO_SKILLS_DIR "${CMAKE_SOURCE_DIR}/fatfs_image/skills"
    MANIFEST_PATH "${CMAKE_BINARY_DIR}/component_skills_manifest.json"
)
```

### 3.4 配置 16MB Flash 分区表

K10 是 16MB Flash，默认 2MB 分区不够。

创建 `partitions.csv`：

```csv
# Name,    Type, SubType, Offset,   Size
nvs,       data, nvs,     0x9000,   0x6000
otadata,   data, ota,     0xF000,   0x2000
phy_init,  data, phy,     0x11000,  0x1000
ota_0,     app,  ota_0,         ,   4M
ota_1,     app,  ota_1,         ,   4M
emote,     data, spiffs,        ,   3M
storage,   data, fat,           ,   4M
```

### 3.5 设置目标芯片和 Flash 大小

```bash
# 清除旧配置
rm -rf build sdkconfig sdkconfig.old

# 设置目标为 ESP32-S3
idf.py set-target esp32s3

# 修改 Flash 大小为 16MB
sed -i '' 's/CONFIG_ESPTOOLPY_FLASHSIZE_2MB=y/# CONFIG_ESPTOOLPY_FLASHSIZE_2MB is not set/' sdkconfig
sed -i '' 's/CONFIG_ESPTOOLPY_FLASHSIZE="2MB"/CONFIG_ESPTOOLPY_FLASHSIZE="16MB"/' sdkconfig
echo 'CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y' >> sdkconfig
```

---

## 四、创建 Unihiker K10 自定义板卡

### 4.1 创建板卡目录结构

```bash
mkdir -p components/unihiker_k10/boards/unihiker_k10
cd components/unihiker_k10/boards/unihiker_k10
```

### 4.2 创建 board_info.yaml

```yaml
board: unihiker_k10
chip: esp32s3
version: '1.0.0'
description: "DFRobot UNIHIKER K10 - ESP32-S3 N16R8 AI Learning Board"
manufacturer: "DFRobot"
```

### 4.3 创建 Kconfig

```kconfig
config BOARD_UNIHIKER_K10
    bool "UNIHIKER K10 (DFRobot ESP32-S3 AI Board)"
    depends on SOC_ESP32S3
    help
        UNIHIKER K10 with 2.8" ILI9341 LCD, GC2145 Camera,
        dual mics, speaker, RGB LEDs, and onboard sensors.
```

### 4.4 创建 board_peripherals.yaml

**注意**：外设名不能含连字符，且 `ledc` 类型需要有效 GPIO。为避免 Board Manager 生成冲突，**留空外设**，后续在代码中手动初始化。

```yaml
board: unihiker_k10
chip: esp32s3
version: '1.0.0'
```

### 4.5 创建 board_devices.yaml

所有设备使用 `custom` 类型，避免 Board Manager 自动生成不兼容的代码。

```yaml
board: unihiker_k10
chip: esp32s3
version: '1.0.0'

devices:
  - name: display_lcd
    type: custom
    init_skip: false
    config:
      driver: ili9341

  - name: fs_spiffs
    type: fs_spiffs
    config:
      base_path: "/spiffs"
      partition_label: "storage"
      max_files: 5

  - name: rgb_led
    type: custom
    init_skip: false
    config:
      driver: ws2812

  - name: xl9535
    type: custom
    init_skip: false
    config:
      driver: tca9555

  - name: camera_gc2145
    type: custom
    init_skip: false
    config:
      driver: gc2145_dvp

  - name: aht20
    type: custom
    init_skip: false
    config:
      driver: aht20

  - name: ltr303
    type: custom
    init_skip: false
    config:
      driver: ltr303als

  - name: sc7a20h
    type: custom
    init_skip: false
    config:
      driver: sc7a20h

  - name: audio_codec
    type: custom
    init_skip: false
    config:
      driver: es8311_es7243e
```

### 4.6 给板卡组件添加 CMakeLists.txt

```bash
cd ~/github/esp-claw/application/basic_demo/components/unihiker_k10
cat > CMakeLists.txt << 'EOF'
idf_component_register()
EOF
```

---

## 五、添加 LCD + 背光初始化代码

### 5.1 创建 `main/k10_display.h`

```c
#ifndef K10_DISPLAY_H
#define K10_DISPLAY_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t k10_display_init(void);

#ifdef __cplusplus
}
#endif

#endif
```

### 5.2 创建 `main/k10_display.c`

```c
#include "k10_display.h"
#include "esp_log.h"
#include "esp_check.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_ili9341.h"

static const char *TAG = "K10_DISP";

#define K10_I2C_PORT        I2C_NUM_1
#define K10_I2C_SDA_PIN     47
#define K10_I2C_SCL_PIN     48
#define K10_XL9535_ADDR     0x20

#define K10_SPI_HOST        SPI3_HOST
#define K10_SPI_MOSI_PIN    21
#define K10_SPI_SCLK_PIN    12
#define K10_LCD_CS_PIN      14
#define K10_LCD_DC_PIN      13

#define K10_LCD_WIDTH       240
#define K10_LCD_HEIGHT      320

#define XL9535_OUTPUT_0     0x02
#define XL9535_CONFIG_0     0x06

static i2c_master_bus_handle_t s_i2c_bus = NULL;
static i2c_master_dev_handle_t s_xl9535_dev = NULL;
static esp_lcd_panel_handle_t  s_lcd_panel = NULL;

static esp_err_t xl9535_write_reg(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_xl9535_dev, buf, 2, -1);
}

static esp_err_t k10_xl9535_init(void)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = K10_XL9535_ADDR,
        .scl_speed_hz = 400000,
    };
    ESP_RETURN_ON_ERROR(i2c_master_bus_add_device(s_i2c_bus, &dev_cfg, &s_xl9535_dev),
                        TAG, "XL9535 add device failed");

    // Pin0=背光, Pin1=LCD复位 -> 设为输出
    ESP_RETURN_ON_ERROR(xl9535_write_reg(XL9535_CONFIG_0, 0xFC),
                        TAG, "XL9535 set dir failed");

    // 先拉低复位，背光灭
    ESP_RETURN_ON_ERROR(xl9535_write_reg(XL9535_OUTPUT_0, 0x00),
                        TAG, "XL9535 reset assert failed");
    vTaskDelay(pdMS_TO_TICKS(50));

    // 开启背光，释放复位
    ESP_RETURN_ON_ERROR(xl9535_write_reg(XL9535_OUTPUT_0, 0x03),
                        TAG, "XL9535 backlight on failed");
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "XL9535 initialized, backlight ON");
    return ESP_OK;
}

static esp_err_t k10_lcd_reset_via_xl9535(void)
{
    ESP_RETURN_ON_ERROR(xl9535_write_reg(XL9535_OUTPUT_0, 0x01),
                        TAG, "LCD reset low failed");
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_RETURN_ON_ERROR(xl9535_write_reg(XL9535_OUTPUT_0, 0x03),
                        TAG, "LCD reset high failed");
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "LCD reset completed");
    return ESP_OK;
}

static esp_err_t k10_lcd_init(void)
{
    spi_bus_config_t buscfg = {
        .mosi_io_num = K10_SPI_MOSI_PIN,
        .miso_io_num = -1,
        .sclk_io_num = K10_SPI_SCLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = K10_LCD_WIDTH * K10_LCD_HEIGHT * sizeof(uint16_t),
    };
    esp_err_t ret = spi_bus_initialize(K10_SPI_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus already initialized, reusing");
    }

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = K10_LCD_CS_PIN,
        .dc_gpio_num = K10_LCD_DC_PIN,
        .spi_mode = 0,
        .pclk_hz = 40 * 1000 * 1000,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_io_spi(K10_SPI_HOST, &io_config, &io_handle),
                        TAG, "LCD IO init failed");

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = -1,
        .rgb_endian = LCD_RGB_ENDIAN_BGR,
        .bits_per_pixel = 16,
    };
    ESP_RETURN_ON_ERROR(esp_lcd_new_panel_ili9341(io_handle, &panel_config, &s_lcd_panel),
                        TAG, "ILI9341 panel init failed");

    ESP_RETURN_ON_ERROR(k10_lcd_reset_via_xl9535(), TAG, "LCD reset failed");

    esp_lcd_panel_init(s_lcd_panel);
    esp_lcd_panel_invert_color(s_lcd_panel, false);
    esp_lcd_panel_swap_xy(s_lcd_panel, false);
    esp_lcd_panel_mirror(s_lcd_panel, false, true);
    esp_lcd_panel_disp_on_off(s_lcd_panel, true);

    ESP_LOGI(TAG, "ILI9341 LCD initialized");

    // 清屏为白色（验证背光状态）
    size_t buf_size = K10_LCD_WIDTH * K10_LCD_HEIGHT * sizeof(uint16_t);
    uint16_t *buffer = heap_caps_calloc(1, buf_size, MALLOC_CAP_DMA);
    if (buffer) {
        for (int i = 0; i < K10_LCD_WIDTH * K10_LCD_HEIGHT; i++) {
            buffer[i] = 0xFFFF;  // 白色
        }
        esp_lcd_panel_draw_bitmap(s_lcd_panel, 0, 0, K10_LCD_WIDTH, K10_LCD_HEIGHT, buffer);
        heap_caps_free(buffer);
        ESP_LOGI(TAG, "LCD cleared to WHITE");
    }

    return ESP_OK;
}

esp_err_t k10_display_init(void)
{
    ESP_LOGI(TAG, "Starting K10 display init...");

    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = K10_I2C_PORT,
        .sda_io_num = K10_I2C_SDA_PIN,
        .scl_io_num = K10_I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .intr_priority = 0,
        .trans_queue_depth = 0,
        .flags = { .enable_internal_pullup = true },
    };
    ESP_RETURN_ON_ERROR(i2c_new_master_bus(&i2c_cfg, &s_i2c_bus),
                        TAG, "I2C master bus init failed");

    ESP_RETURN_ON_ERROR(k10_xl9535_init(), TAG, "XL9535 init failed");
    ESP_RETURN_ON_ERROR(k10_lcd_init(), TAG, "LCD init failed");

    ESP_LOGI(TAG, "K10 display init completed!");
    return ESP_OK;
}
```

### 5.3 在 `main.c` 中调用

确保 `main.c` 包含：

```c
#include "k10_display.h"
```

并在 `app_main()` 中调用：

```c
ESP_ERROR_CHECK(k10_display_init());
```

### 5.4 添加 `main/idf_component.yml`

```yaml
dependencies:
  espressif/esp_lcd_ili9341: "~1.2.0"
```

---

## 六、编译与烧录

### 6.1 生成 Board Manager 配置

```bash
cd ~/github/esp-claw/application/basic_demo

# 设置环境变量（每次新终端都需要）
export IDF_EXTRA_ACTIONS_PATH=~/github/esp-claw/application/basic_demo/managed_components/espressif__esp_board_manager

# 生成 K10 板卡配置
idf.py gen-bmgr-config -b unihiker_k10
```

### 6.2 编译

```bash
idf.py build
```

### 6.3 烧录

K10 使用靠近 Reset 键的 USB 口（USB Serial JTAG）：

```bash
# macOS 下设备名通常为 /dev/cu.usbmodemXXXX
idf.py -p /dev/cu.usbmodem2201 flash monitor
```

> **注意**：如果串口无法输入，在 `menuconfig` 中将控制台输出改为 USB Serial JTAG：
> ```bash
> idf.py menuconfig
> # → ESP System Settings → Channel for console output → USB Serial/JTAG Controller
> ```

---

## 七、验证结果

### 7.1 串口日志预期输出

```
I (xxx) K10_DISP: Starting K10 display init...
I (xxx) K10_DISP: XL9535 initialized, backlight ON
I (xxx) K10_DISP: LCD reset completed
I (xxx) K10_DISP: ILI9341 LCD initialized
I (xxx) K10_DISP: LCD cleared to WHITE
I (xxx) K10_DISP: K10 display init completed!
I (xxx) basic_demo: Starting console REPL
Type 'help', 'auto rules', 'auto last', or 'auto emit_message qq_gateway qq 123 hello'
```

### 7.2 屏幕状态
- **背光亮起** ✅
- **屏幕显示白色** ✅（确认 ILI9341 初始化成功）

---

## 八、常见问题与解决

| 问题 | 原因 | 解决 |
|-----|------|------|
| `Submodule update failed: Failed to obtain credentials` | EIM 使用 jihulab 镜像 | 改用命令行 `git clone` 安装 |
| `partitions.csv missing` | 未创建自定义分区表 | 创建 16MB 分区表文件 |
| `gen_bmgr_codes missing` | Board Manager 未执行 | 运行 `idf.py gen-bmgr-config -b unihiker_k10` |
| `Flash size 2MB` 报错 | sdkconfig 被重置 | 修改 `CONFIG_ESPTOOLPY_FLASHSIZE` 为 16MB |
| `claw_cap could not be found` | 组件搜索路径缺失 | 在 `CMakeLists.txt` 中添加 `EXTRA_COMPONENT_DIRS` |
| `esp_lcd_touch` 缺失 | lua_module_lcd_touch 依赖 | `idf.py add-dependency espressif/esp_lcd_touch` |
| `expression_emote.h` 缺失 | K10 不需要表情组件 | 删除 `app_expression_emote.c/h` 后 `fullclean` |
| 屏幕亮一下又灭 | `esp_lcd_panel_reset()` 干扰 XL9535 | 移除 `esp_lcd_panel_reset()`，改用手动 XL9535 复位 |
| 串口无法输入 | 控制台输出为 UART | `menuconfig` 改为 USB Serial/JTAG Controller |

---

## 九、后续扩展

当前以下硬件尚未驱动，可在 `k10_display.c` 基础上继续扩展：

| 硬件 | 驱动方案 |
|-----|---------|
| **GC2145 摄像头** | 参考 `esp32-camera` 驱动，使用 DVP 接口 |
| **WS2812 RGB LED** | 使用 `led_strip` 组件或 RMT 驱动 |
| **AHT20 / LTR303 / SC7A20H** | 使用对应传感器驱动库 |
| **ES8311 + ES7243E 音频** | 参考 xiaozhi-esp32 `k10_audio_codec.cc` |

---

## 十、参考资源

- [ESP-Claw GitHub](https://github.com/espressif/esp-claw)
- [xiaozhi-esp32 K10 配置](https://github.com/78/xiaozhi-esp32/tree/main/main/boards/df-k10) — 引脚定义参考
- [ESP-IDF Build System](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html)
- [ESP Board Manager](https://components.espressif.com/components/espressif/esp_board_manager)

---

*文档生成时间：2026-04-22*
*适用版本：ESP-IDF v5.5.4 + ESP-Claw (master branch)*
