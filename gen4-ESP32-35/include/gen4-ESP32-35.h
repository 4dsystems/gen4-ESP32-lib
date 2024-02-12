/*
 * 4D Systems Pty Ltd
 * www.4dsystems.com.au
 *
 * SPDX-FileCopyrightText: 
 *   - 2022-2023 Espressif Systems (Shanghai) CO LTD
 *   - 4D Systems Pty Ltd
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @file
 * @brief ESP LCD: gen4-ESP32-35 Series
 */

#pragma once

#include "esp_lcd_panel_vendor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ILI9488_INTRFC_MODE_CTL                     0xB0
#define ILI9488_FRAME_RATE_NORMAL_CTL               0xB1
#define ILI9488_INVERSION_CTL                       0xB4
#define ILI9488_FUNCTION_CTL                        0xB6
#define ILI9488_ENTRY_MODE_CTL                      0xB7
#define ILI9488_POWER_CTL_ONE                       0xC0
#define ILI9488_POWER_CTL_TWO                       0xC1
#define ILI9488_POWER_CTL_THREE                     0xC5
#define ILI9488_POSITIVE_GAMMA_CTL                  0xE0
#define ILI9488_NEGATIVE_GAMMA_CTL                  0xE1
#define ILI9488_ADJUST_CTL_THREE                    0xF7
#define ILI9488_COLOR_MODE_16BIT                    0x55
#define ILI9488_COLOR_MODE_18BIT                    0x66
#define ILI9488_INTERFACE_MODE_USE_SDO              0x00
#define ILI9488_INTERFACE_MODE_IGNORE_SDO           0x80
#define ILI9488_IMAGE_FUNCTION_DISABLE_24BIT_DATA   0x00
#define ILI9488_WRITE_MODE_BCTRL_DD_ON              0x28
#define ILI9488_FRAME_RATE_60HZ                     0xA0
#define ILI9488_INIT_LENGTH_MASK                    0x1F
#define ILI9488_INIT_DONE_FLAG                      0xFF
#define ILI9488_MADCTL                              0x36
#define ILI9488_PIXFMT                              0x3A
#define ILI9488_CMD_SLEEP_OUT                       0x11
#define ILI9488_CMD_DISPLAY_OFF                     0x28
#define ILI9488_CMD_DISPLAY_ON                      0x29
#define ILI9488_SET_IMAGE_FUNCTION                  0xE9
#define ILI9488_CMD_SLEEP_OUT                       0x11
#define ILI9488_CMD_DISPLAY_OFF                     0x28
#define ILI9488_CMD_DISPLAY_ON                      0x29
#define ILI9488_CMD_DISP_INVERSION_OFF              0x20
#define ILI9488_CMD_DISP_INVERSION_ON               0x21
#define ILI9488_CMD_COLUMN_ADDRESS_SET              0x2A
#define ILI9488_CMD_PAGE_ADDRESS_SET                0x2B
#define ILI9488_CMD_MEMORY_WRITE                    0x2C

/**
 * @brief LCD panel initialization commands.
 *
 */
typedef struct {
    int cmd;                /*<! The specific LCD command */
    const void *data;       /*<! Buffer that holds the command specific data */
    size_t data_bytes;      /*<! Size of `data` in memory, in bytes */
    unsigned int delay_ms;  /*<! Delay in milliseconds after this command */
} gen4_esp32_35_lcd_init_cmd_t;

/**
 * @brief LCD panel vendor configuration.
 *
 * @note  This structure needs to be passed to the `vendor_config` field in `esp_lcd_panel_dev_config_t`.
 *
 */
typedef struct {
    const gen4_esp32_35_lcd_init_cmd_t *init_cmds;      /*!< Pointer to initialization commands array. Set to NULL if using default commands.
                                                         *   The array should be declared as `static const` and positioned outside the function.
                                                         *   Please refer to `vendor_specific_init_default` in source file.
                                                         */
    uint16_t init_cmds_size;                            /*<! Number of commands in above array */
} gen4_esp32_35_vendor_config_t;

/**
 * @brief Create LCD panel for gen4-ESP32-35 series of displays
 *
 * @note  Vendor specific initialization can be different between manufacturers, should consult the LCD supplier for initialization sequence code.
 *
 * @param[in] io LCD panel IO handle
 * @param[in] panel_dev_config general panel device configuration
 * @param[out] ret_panel Returned LCD panel handle
 * @return
 *          - ESP_ERR_INVALID_ARG   if parameter is invalid
 *          - ESP_ERR_NO_MEM        if out of memory
 *          - ESP_OK                on success
 */
esp_err_t esp_lcd_new_gen4_esp32_35(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel);

/**
 * @brief LCD panel bus configuration structure
 *
 * @param[in] max_trans_sz Maximum transfer size in bytes
 *
 */
#define GEN4_ESP32_35_BUS_SPI_CONFIG(max_trans_sz)              \
    {                                                           \
        .sclk_io_num = 14,                                      \
        .mosi_io_num = 13,                                      \
        .miso_io_num = 12,                                      \
        .quadhd_io_num = -1,                                    \
        .quadwp_io_num = -1,                                    \
        .max_transfer_sz = max_trans_sz,                        \
    }

/**
 * @brief LCD panel IO configuration structure
 *
 * @param[in] cb Callback function when SPI transfer is done
 * @param[in] cb_ctx Callback function context
 *
 */
#define GEN4_ESP32_35_IO_SPI_CONFIG(callback, callback_ctx)     \
    {                                                           \
        .cs_gpio_num = -1,                                      \
        .dc_gpio_num = 21,                                      \
        .spi_mode = 0,                                          \
        .pclk_hz = 40 * 1000 * 1000,                            \
        .trans_queue_depth = 7,                                 \
        .on_color_trans_done = callback,                        \
        .user_ctx = callback_ctx,                               \
        .lcd_cmd_bits = 8,                                      \
        .lcd_param_bits = 8,                                    \
    }

#ifdef __cplusplus
}
#endif
