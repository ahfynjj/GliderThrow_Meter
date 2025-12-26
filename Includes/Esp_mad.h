/**
 * 
 *  @file       Esp_mad.h
 *  @author     Alain Désandré - alain.desandre@wanadoo.fr
 *  @version    1.0
 *  @date       28 octobre 2018
 *  @brief      Various define used by esp-mad project
 *  @details    This file is shared by Esp_mad_server and Esp_mad_clients
 * 
 *              Version History :
 *                  - 23/05/2019 - V1.0.1 - new gpio management
 */
 
#ifndef  _ESP_MAD_H_
#define _ESP_MAD_H_

#define PI  3.14159265359

#define AP_WIFI_SSID    "ESP_MAD" 

/*
 * Board selection: ESP32-C3 only
 * - Default pins chosen to avoid strapping pins and keep GPIO8 free (often wired to RGB LED on C3 devkits).
 * - Override PIN_SDA / PIN_CLK / BLINK_GPIO at compile time if你的硬件接线不同。
 */
#if CONFIG_IDF_TARGET_ESP32C3
    #define ESP_MAD_DEFAULT_SDA_GPIO    5
    #define ESP_MAD_DEFAULT_SCL_GPIO    6
    #define ESP_MAD_DEFAULT_LED_GPIO    8
    #define ESP_MAD_TARGET_LED_GPIO     0
    #define ESP_MAD_DEFAULT_BATT_CH     1
#endif

#ifndef PIN_SDA
    #define PIN_SDA ESP_MAD_DEFAULT_SDA_GPIO
#endif

#ifndef PIN_CLK
    #define PIN_CLK ESP_MAD_DEFAULT_SCL_GPIO
#endif

#ifndef BLINK_GPIO
    #define BLINK_GPIO  (gpio_num_t)ESP_MAD_DEFAULT_LED_GPIO
#endif

#ifndef TARGET_LED_GPIO
    #define TARGET_LED_GPIO  (gpio_num_t)ESP_MAD_TARGET_LED_GPIO
#endif

#endif /* _ESP_MAD_H_ */
 