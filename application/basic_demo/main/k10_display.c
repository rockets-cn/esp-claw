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

    // Pin0=背光, Pin1=LCD复位 -> 设为输出 (11111100)
    ESP_RETURN_ON_ERROR(xl9535_write_reg(XL9535_CONFIG_0, 0xFC),
                        TAG, "XL9535 set dir failed");
    
    // 先拉低复位，背光保持灭
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
    // 尝试初始化 SPI，如果已存在则忽略错误
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
    
    // 手动复位 LCD
    ESP_RETURN_ON_ERROR(k10_lcd_reset_via_xl9535(), TAG, "LCD reset failed");
    
    esp_lcd_panel_init(s_lcd_panel);
    esp_lcd_panel_invert_color(s_lcd_panel, false);
    esp_lcd_panel_swap_xy(s_lcd_panel, false);
    esp_lcd_panel_mirror(s_lcd_panel, false, true);
    esp_lcd_panel_disp_on_off(s_lcd_panel, true);
    
    ESP_LOGI(TAG, "ILI9341 LCD initialized");
    
    // 清屏为白色（确认背光状态）
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
