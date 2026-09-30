#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "ssd1306.h"

static const char *TAG = "OLED_HELTEC_V4";

#define I2C_SDA_PIN     17
#define I2C_SCL_PIN     18
#define OLED_RST_PIN    21
#define VEXT_PIN        36

#define MAX_LINE_LEN    20
#define MAX_LINES       5

static char lines[MAX_LINES][MAX_LINE_LEN + 1];
static int  current_line = 0;
static int  current_col  = 0;

static void redraw(ssd1306_handle_t disp)
{
    ssd1306_clear(disp);
    for (int i = 0; i < MAX_LINES; i++) {
        ssd1306_draw_text(disp, 0, i * 12, lines[i], true);
    }
    ssd1306_display(disp);
}

static void add_char(ssd1306_handle_t disp, char c)
{
    if (current_col >= MAX_LINE_LEN) {
        current_col = 0;
        current_line++;
        if (current_line >= MAX_LINES) {
            for (int i = 0; i < MAX_LINES - 1; i++) {
                memcpy(lines[i], lines[i + 1], MAX_LINE_LEN + 1);
            }
            memset(lines[MAX_LINES - 1], 0, MAX_LINE_LEN + 1);
            current_line = MAX_LINES - 1;
        }
    }
    lines[current_line][current_col++] = c;
    lines[current_line][current_col] = '\0';
    redraw(disp);
}

static void new_line(ssd1306_handle_t disp)
{
    current_col = 0;
    current_line++;
    if (current_line >= MAX_LINES) {
        for (int i = 0; i < MAX_LINES - 1; i++) {
            memcpy(lines[i], lines[i + 1], MAX_LINE_LEN + 1);
        }
        memset(lines[MAX_LINES - 1], 0, MAX_LINE_LEN + 1);
        current_line = MAX_LINES - 1;
    } else {
        memset(lines[current_line], 0, MAX_LINE_LEN + 1);
    }
    redraw(disp);
}

static void backspace(ssd1306_handle_t disp)
{
    if (current_col > 0) {
        current_col--;
        lines[current_line][current_col] = '\0';
        redraw(disp);
    } else if (current_line > 0) {
        current_line--;
        current_col = strlen(lines[current_line]);
        redraw(disp);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Инициализация OLED для Heltec V4 ===");

    // 1. Питание дисплея (Vext = LOW для Heltec V4)
    gpio_config_t vext_cfg = {
        .pin_bit_mask = (1ULL << VEXT_PIN),
        .mode = GPIO_MODE_OUTPUT,
    };
    gpio_config(&vext_cfg);
    gpio_set_level(VEXT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(200));

    // 2. I2C
    i2c_master_bus_config_t i2c_cfg = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_PIN,
        .scl_io_num = I2C_SCL_PIN,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t bus_handle;
    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_cfg, &bus_handle));

    // 3. Дисплей SSD1306/SSD1315
    ssd1306_config_t cfg = {
        .bus = SSD1306_I2C,
        .width = 128,
        .height = 64,
        .iface.i2c = {
            .port = I2C_NUM_0,
            .addr = 0x3C,
            .rst_gpio = OLED_RST_PIN,
        },
    };
    ssd1306_handle_t disp;
    ssd1306_new_i2c(&cfg, &disp);
    ssd1306_clear(disp);

    // 4. Стартовое сообщение
    memset(lines, 0, sizeof(lines));
    strcpy(lines[0], "Type now:");
    current_line = 1;
    current_col  = 0;
    redraw(disp);

    // 5. Установка драйвера USB Serial/JTAG для прямого чтения
    usb_serial_jtag_driver_config_t usb_cfg = USB_SERIAL_JTAG_DRIVER_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(usb_serial_jtag_driver_install(&usb_cfg));

    ESP_LOGI(TAG, "Готово. Пиши символы — они появятся на экране сразу.");

    // 6. Основной цикл — прямое чтение с USB
    uint8_t buf[64];
    while (1) {
        int len = usb_serial_jtag_read_bytes(buf, sizeof(buf), pdMS_TO_TICKS(50));
        if (len > 0) {
            for (int i = 0; i < len; i++) {
                uint8_t c = buf[i];
                if (c == '\r' || c == '\n') {
                    new_line(disp);
                } else if (c == 0x7F || c == 0x08) {
                    backspace(disp);
                } else if (c >= 0x20 && c <= 0x7E) {
                    add_char(disp, (char)c);
                }
            }
        }
    }
}