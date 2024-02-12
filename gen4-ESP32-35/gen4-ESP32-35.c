/*
 * 4D Systems Pty Ltd
 * www.4dsystems.com.au
 *
 * SPDX-FileCopyrightText: 
 *   - 2022-2023 Espressif Systems (Shanghai) CO LTD
 *   - 4D Systems Pty Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

#include "gen4-ESP32-35.h"

static const char *TAG = "gen4-ESP32-35";

static esp_err_t gen4_esp32_35_del(esp_lcd_panel_t *panel);
static esp_err_t gen4_esp32_35_reset(esp_lcd_panel_t *panel);
static esp_err_t gen4_esp32_35_init(esp_lcd_panel_t *panel);
static esp_err_t gen4_esp32_35_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t gen4_esp32_35_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t gen4_esp32_35_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t gen4_esp32_35_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t gen4_esp32_35_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t gen4_esp32_35_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    uint8_t fb_bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_val; // save current value of LCD_CMD_COLMOD register
    const gen4_esp32_35_lcd_init_cmd_t *init_cmds;
    uint16_t init_cmds_size;
} gen4_esp32_35_panel_t;

esp_err_t esp_lcd_new_gen4_esp32_35(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    gen4_esp32_35_panel_t *gen4_esp32_35 = NULL;
    gpio_config_t io_conf = { 0 };

    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    gen4_esp32_35 = (gen4_esp32_35_panel_t *)calloc(1, sizeof(gen4_esp32_35_panel_t));
    ESP_GOTO_ON_FALSE(gen4_esp32_35, ESP_ERR_NO_MEM, err, TAG, "no mem for gen4_esp32_35 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        io_conf.mode = GPIO_MODE_OUTPUT;
        io_conf.pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num;
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->rgb_endian) {
    case LCD_RGB_ENDIAN_RGB:
        gen4_esp32_35->madctl_val = 0;
        break;
    case LCD_RGB_ENDIAN_BGR:
        gen4_esp32_35->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported rgb endian");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 18: // RGB666
        gen4_esp32_35->colmod_val = 0x66;
        // each color component (R/G/B) should occupy the 6 high bits of a byte, which means 3 full bytes are required for a pixel
        gen4_esp32_35->fb_bits_per_pixel = 24;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    gen4_esp32_35->io = io;
    gen4_esp32_35->reset_gpio_num = panel_dev_config->reset_gpio_num;
    gen4_esp32_35->reset_level = panel_dev_config->flags.reset_active_high;
    if (panel_dev_config->vendor_config) {
        gen4_esp32_35->init_cmds = ((gen4_esp32_35_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds;
        gen4_esp32_35->init_cmds_size = ((gen4_esp32_35_vendor_config_t *)panel_dev_config->vendor_config)->init_cmds_size;
    }
    gen4_esp32_35->base.del = gen4_esp32_35_del;
    gen4_esp32_35->base.reset = gen4_esp32_35_reset;
    gen4_esp32_35->base.init = gen4_esp32_35_init;
    gen4_esp32_35->base.draw_bitmap = gen4_esp32_35_draw_bitmap;
    gen4_esp32_35->base.invert_color = gen4_esp32_35_invert_color;
    gen4_esp32_35->base.set_gap = gen4_esp32_35_set_gap;
    gen4_esp32_35->base.mirror = gen4_esp32_35_mirror;
    gen4_esp32_35->base.swap_xy = gen4_esp32_35_swap_xy;
    gen4_esp32_35->base.disp_on_off = gen4_esp32_35_disp_on_off;

    *ret_panel = &(gen4_esp32_35->base);
    ESP_LOGD(TAG, "new gen4_esp32_35 panel @%p", gen4_esp32_35);

    ESP_LOGI(TAG, "LCD panel create success"); // , version: %d.%d.%d", ESP_LCD_GEN4_ESP32_35_VER_MAJOR, ESP_LCD_GEN4_ESP32_35_VER_MINOR, ESP_LCD_GEN4_ESP32_35_VER_PATCH

    return ESP_OK;

err:
    if (gen4_esp32_35) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(gen4_esp32_35);
    }
    return ret;
}

static esp_err_t gen4_esp32_35_del(esp_lcd_panel_t *panel)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);

    if (gen4_esp32_35->reset_gpio_num >= 0) {
        gpio_reset_pin(gen4_esp32_35->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del gen4_esp32_35 panel @%p", gen4_esp32_35);
    free(gen4_esp32_35);
    return ESP_OK;
}

static esp_err_t gen4_esp32_35_reset(esp_lcd_panel_t *panel)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);
    esp_lcd_panel_io_handle_t io = gen4_esp32_35->io;

    // perform hardware reset
    if (gen4_esp32_35->reset_gpio_num >= 0) {
        gpio_set_level(gen4_esp32_35->reset_gpio_num, gen4_esp32_35->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(gen4_esp32_35->reset_gpio_num, !gen4_esp32_35->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else { // perform software reset
        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5ms before sending new command
    }

    return ESP_OK;
}

static const gen4_esp32_35_lcd_init_cmd_t vendor_specific_init_default[] = {
    {ILI9488_POSITIVE_GAMMA_CTL,    (uint8_t []){0x00, 0x13, 0x18, 0x04, 0x0F, 0x06, 0x3A, 0x56, 0x4D, 0x03, 0x0A, 0x06, 0x30, 0x3E, 0x0F}, 15, 0},
    {ILI9488_NEGATIVE_GAMMA_CTL,    (uint8_t []){0x00, 0x13, 0x18, 0x01, 0x11, 0x06, 0x38, 0x34, 0x4D, 0x06, 0x0D, 0x0B, 0x31, 0x37, 0x0F}, 15, 0},
    {ILI9488_POWER_CTL_ONE,         (uint8_t []){0x18, 0x16}, 2, 0},
    {ILI9488_POWER_CTL_TWO,         (uint8_t []){0x45}, 1, 0},
    {ILI9488_POWER_CTL_THREE,       (uint8_t []){0x00, 0x63, 0x01}, 3, 0},
    {ILI9488_MADCTL,                (uint8_t []){0x48}, 1, 0},
    {ILI9488_PIXFMT,                (uint8_t []){ILI9488_COLOR_MODE_18BIT}, 1, 0},
    {ILI9488_INTRFC_MODE_CTL,       (uint8_t []){ILI9488_INTERFACE_MODE_USE_SDO}, 1, 0},
    {ILI9488_FRAME_RATE_NORMAL_CTL, (uint8_t []){0xB0}, 1, 0},
    {ILI9488_INVERSION_CTL,         (uint8_t []){0x02}, 1, 0},
    {ILI9488_FUNCTION_CTL,          (uint8_t []){0x02, 0x02}, 2, 0},
    {ILI9488_SET_IMAGE_FUNCTION,    (uint8_t []){0x00}, 1, 0},
    {ILI9488_ADJUST_CTL_THREE,      (uint8_t []){0xA9, 0x51, 0x2C, 0x82}, 4, 120},
    {ILI9488_CMD_SLEEP_OUT,         NULL, 0, 120},
    {ILI9488_CMD_DISPLAY_ON,        NULL, 0, 120},
    {ILI9488_CMD_DISP_INVERSION_ON, NULL, 0, 120},
};

static esp_err_t gen4_esp32_35_init(esp_lcd_panel_t *panel)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);
    esp_lcd_panel_io_handle_t io = gen4_esp32_35->io;

    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0), TAG, "send command failed");
    vTaskDelay(pdMS_TO_TICKS(100));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        gen4_esp32_35->madctl_val,
    }, 1), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        gen4_esp32_35->colmod_val,
    }, 1), TAG, "send command failed");

    const gen4_esp32_35_lcd_init_cmd_t *init_cmds = NULL;
    uint16_t init_cmds_size = 0;
    if (gen4_esp32_35->init_cmds) {
        init_cmds = gen4_esp32_35->init_cmds;
        init_cmds_size = gen4_esp32_35->init_cmds_size;
    } else {
        init_cmds = vendor_specific_init_default;
        init_cmds_size = sizeof(vendor_specific_init_default) / sizeof(gen4_esp32_35_lcd_init_cmd_t);
    }

    bool is_cmd_overwritten = false;
    for (int i = 0; i < init_cmds_size; i++) {
        // Check if the command has been used or conflicts with the internal
        switch (init_cmds[i].cmd) {
        case LCD_CMD_MADCTL:
            is_cmd_overwritten = true;
            gen4_esp32_35->madctl_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        case LCD_CMD_COLMOD:
            is_cmd_overwritten = true;
            gen4_esp32_35->colmod_val = ((uint8_t *)init_cmds[i].data)[0];
            break;
        default:
            is_cmd_overwritten = false;
            break;
        }

        if (is_cmd_overwritten) {
            ESP_LOGW(TAG, "The %02Xh command has been used and will be overwritten by external initialization sequence", init_cmds[i].cmd);
        }

        ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, init_cmds[i].cmd, init_cmds[i].data, init_cmds[i].data_bytes), TAG, "send command failed");
        vTaskDelay(pdMS_TO_TICKS(init_cmds[i].delay_ms));
    }
    ESP_LOGD(TAG, "send init commands success");

    return ESP_OK;
}

static esp_err_t gen4_esp32_35_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = gen4_esp32_35->io;

    x_start += gen4_esp32_35->x_gap;
    x_end += gen4_esp32_35->x_gap;
    y_start += gen4_esp32_35->y_gap;
    y_end += gen4_esp32_35->y_gap;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_CASET, (uint8_t[]) {
        (x_start >> 8) & 0xFF,
        x_start & 0xFF,
        ((x_end - 1) >> 8) & 0xFF,
        (x_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_RASET, (uint8_t[]) {
        (y_start >> 8) & 0xFF,
        y_start & 0xFF,
        ((y_end - 1) >> 8) & 0xFF,
        (y_end - 1) & 0xFF,
    }, 4), TAG, "send command failed");
    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * gen4_esp32_35->fb_bits_per_pixel / 8;
    esp_lcd_panel_io_tx_color(io, LCD_CMD_RAMWR, color_data, len);

    return ESP_OK;
}

static esp_err_t gen4_esp32_35_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);
    esp_lcd_panel_io_handle_t io = gen4_esp32_35->io;
    int command = 0;
    if (invert_color_data) {
        command = ILI9488_CMD_DISP_INVERSION_ON; //LCD_CMD_INVON;
    } else {
        command = ILI9488_CMD_DISP_INVERSION_OFF; //LCD_CMD_INVOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t gen4_esp32_35_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);
    esp_lcd_panel_io_handle_t io = gen4_esp32_35->io;
    if (mirror_x) {
        gen4_esp32_35->madctl_val |= LCD_CMD_MX_BIT;
    } else {
        gen4_esp32_35->madctl_val &= ~LCD_CMD_MX_BIT;
    }
    if (mirror_y) {
        gen4_esp32_35->madctl_val |= LCD_CMD_MY_BIT;
    } else {
        gen4_esp32_35->madctl_val &= ~LCD_CMD_MY_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        gen4_esp32_35->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t gen4_esp32_35_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);
    esp_lcd_panel_io_handle_t io = gen4_esp32_35->io;
    if (swap_axes) {
        gen4_esp32_35->madctl_val |= LCD_CMD_MV_BIT;
    } else {
        gen4_esp32_35->madctl_val &= ~LCD_CMD_MV_BIT;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        gen4_esp32_35->madctl_val
    }, 1), TAG, "send command failed");
    return ESP_OK;
}

static esp_err_t gen4_esp32_35_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);
    gen4_esp32_35->x_gap = x_gap;
    gen4_esp32_35->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t gen4_esp32_35_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    gen4_esp32_35_panel_t *gen4_esp32_35 = __containerof(panel, gen4_esp32_35_panel_t, base);
    esp_lcd_panel_io_handle_t io = gen4_esp32_35->io;
    int command = 0;
    if (on_off) {
        command = LCD_CMD_DISPON;
    } else {
        command = LCD_CMD_DISPOFF;
    }
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, command, NULL, 0), TAG, "send command failed");
    return ESP_OK;
}