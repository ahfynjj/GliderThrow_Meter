/**
 * @file    esp_mad.cpp
 * @author  Alain Désandré
 * @date    28 octobre 2018
 * @brief   esp_mad project main files.
 *
 * @details The MAD application is intended to provide a ESP32-based tool
 *          for visualizing the deflection (in mm and degree) of control surface
 *          on a remote-controlled model.
 *          ESP_MAD is composed of six files :
 *              . esp_mad.ccp : main function.
 *              . esp_mad_task_measure.cpp : MPU6050 management
 *              . esp_mad_task_http_server.c : http server
 *              . Esp_mad.h : Globals define
 *              . Esp_mad_Globals_Variables.h : Globals variables
 *              . esp_html.h : HTML page and Jquery/Javascript function.
 * 
 */

/*-----------------------------------------
 *-            INCLUDES        
 *-----------------------------------------*/
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include <esp_log.h>
#include <esp_err.h>
#include "driver/gpio.h"
#include "led_strip.h"
#include <math.h>
#include <Esp_mad.h>

/*-----------------------------------------
 * GLOBALS VARIABLES DECLARATION & INIT.        
 *-----------------------------------------*/
#define DEFINE_VARIABLES
#include <Esp_mad_Globals_Variables.h>

/*-----------------------------------------
 *-            EXTERNALS        
 *-----------------------------------------*/
extern "C" {
	void app_main(void);
    void task_http_server(void*);
    void task_vBattery(void*);
}

extern void task_measure(void*);
extern void task_http_server(void*);
static void task_target_led(void *ignore);

/**
 *	@fn 	    app_main(void)
 *	@brief 		esp_mad project entry point function.
 *	@param[in]	void*
 *	@return		void.
 */
void app_main(void)
{
    /*--- Locals declaration ---*/
    int iDelay = 100;           /* Used to setup the blinky period when MPU6050 is not calibrate */

    /*--- two tasks are launched. One task handle the MPU6050 measurement and   ---*/
    /*--- the other one is a pretty simple http server to deal with the browser ---*/
    /*--- requests.										                        ---*/
    xTaskCreate(&task_measure, "measure_task", 8192, NULL, 5, NULL);

	/*--- Wait for stabilisation, not mandatory ---*/
    vTaskDelay(500/portTICK_PERIOD_MS);

    xTaskCreate(&task_http_server, "http_server_task", 8192, NULL, 5, NULL);

    /*--- Wait for stabilisation, not mandatory ---*/
    vTaskDelay(500/portTICK_PERIOD_MS);

    xTaskCreate(&task_vBattery, "vBattery_task", 8192, NULL, 5, NULL);

    xTaskCreate(&task_target_led, "target_led_task", 2048, NULL, 4, NULL);

    /*---  Configure the IOMUX register for pad BLINK_GPIO (some pads are ---*/
    /*---  muxed to GPIO on reset already, but some default to other      ---*/
    /*---  functions and need to be switched to GPIO. Consult the         ---*/
    /*---  Technical Reference for a list of pads and their default       ---*/
    /*---  functions.). Blinky led.                                       ---*/
    gpio_config_t io_conf;
    //disable interrupt
    io_conf.intr_type = (gpio_int_type_t)GPIO_INTR_DISABLE;
    //set as output mode
    io_conf.mode = GPIO_MODE_OUTPUT;
    //bit mask of the pins that you want to set
    io_conf.pin_bit_mask = (1ULL<<BLINK_GPIO);
    //disable pull-down mode
    io_conf.pull_down_en = (gpio_pulldown_t)0;
    //disable pull-up mode
    io_conf.pull_up_en = (gpio_pullup_t)0;
    //configure GPIO with the given settings
    gpio_config(&io_conf);
 
	while(1) 
	{
        /*--- Blinky period set to 100 ms before calibration 2s after ---*/
        if(BInit && (iDelay != 2000))
            iDelay = 2000;

        /* Blink off (output low) */
        gpio_set_level((gpio_num_t)BLINK_GPIO, 0);
		
        vTaskDelay(pdMS_TO_TICKS(iDelay));
		
        /* Blink on (output high) */
        gpio_set_level((gpio_num_t)BLINK_GPIO, 1);
		
        vTaskDelay(pdMS_TO_TICKS(iDelay));

    } /* End while */

} /* end app_main() */

static void task_target_led(void *ignore)
{
    static const char *TAG = "target_led";

    led_strip_config_t strip_config = {
        .strip_gpio_num = TARGET_LED_GPIO,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = {
            .invert_out = false,
        },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,
        .mem_block_symbols = 64,
        .flags = {
            .with_dma = false,
        },
    };

    led_strip_handle_t strip;
    esp_err_t err = led_strip_new_rmt_device(&strip_config, &rmt_config, &strip);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to init led strip: %s", esp_err_to_name(err));
        vTaskDelete(NULL);
        return;
    }

    led_strip_clear(strip);

    enum class target_color
    {
        OFF,
        GREEN,
        BLUE,
        CYAN,
        YELLOW,
        ORANGE,
        RED
    };

    auto apply_color = [&](target_color color)
    {
        static target_color last_color = target_color::OFF;
        if (color == last_color)
        {
            return;
        }
        last_color = color;

        auto set_pixel = [&](uint8_t r, uint8_t g, uint8_t b)
        {
            led_strip_set_pixel(strip, 0, r, g, b);
            led_strip_refresh(strip);
        };

        switch (color)
        {
            case target_color::OFF:
                set_pixel(0, 0, 0);
                break;
            case target_color::GREEN:
                set_pixel(0, 64, 0);
                break;
            case target_color::BLUE:
                set_pixel(0, 0, 64);
                break;
            case target_color::CYAN:
                set_pixel(0, 32, 64);
                break;
            case target_color::YELLOW:
                set_pixel(64, 48, 0);
                break;
            case target_color::ORANGE:
                set_pixel(64, 16, 0);
                break;
            case target_color::RED:
                set_pixel(64, 0, 0);
                break;
        }
    };

    while (1)
    {
        if (!g_targetAngleActive || !BInit)
        {
            apply_color(target_color::OFF);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        float relativeAngle = g_angle - g_angleZeroOffset;
        float diff = fabsf(relativeAngle - g_targetAngle);

        if (diff <= 0.1f)
        {
            apply_color(target_color::GREEN);
        }
        else if (diff <= 0.5f)
        {
            apply_color(target_color::BLUE);
        }
        else if (diff <= 1.0f)
        {
            apply_color(target_color::CYAN);
        }
        else if (diff <= 2.0f)
        {
            apply_color(target_color::YELLOW);
        }
        else if (diff <= 5.0f)
        {
            apply_color(target_color::ORANGE);
        }
        else
        {
            apply_color(target_color::RED);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}