#include "status_led.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"

static const char *TAG = "led";

/* Kept dim on purpose: the LED sits on top of your monitor. */
#define LEVEL 10

static led_strip_handle_t s_strip;
static volatile led_color_t s_color = LED_OFF;
static volatile led_mode_t s_mode = LED_SOLID;
static volatile led_color_t s_flash_color;
static volatile int s_flash_count;

static void show(led_color_t c)
{
    if (!s_strip) {
        return;
    }
    uint32_t r = 0, g = 0, b = 0;
    switch (c) {
    case LED_GREEN: g = LEVEL; break;
    case LED_BLUE: b = LEVEL; break;
    case LED_AMBER: r = LEVEL; g = LEVEL / 3; break;
    case LED_RED: r = LEVEL; break;
    case LED_PURPLE: r = LEVEL / 2; b = LEVEL; break;
    case LED_CYAN: g = LEVEL; b = LEVEL; break;
    case LED_WHITE: r = g = b = LEVEL * 2; break;
    default: break;
    }
    led_strip_set_pixel(s_strip, 0, r, g, b);
    led_strip_refresh(s_strip);
}

static void led_task(void *arg)
{
    (void)arg;
    uint32_t tick = 0;
    for (;;) {
        if (s_flash_count > 0) {
            const led_color_t c = s_flash_color;
            const int n = s_flash_count;
            s_flash_count = 0;
            for (int i = 0; i < n; ++i) {
                show(c);
                vTaskDelay(pdMS_TO_TICKS(150));
                show(LED_OFF);
                vTaskDelay(pdMS_TO_TICKS(200));
            }
        }
        const bool on = s_mode == LED_SOLID || (tick % 10) < 5;
        show(on ? s_color : LED_OFF);
        tick++;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

esp_err_t status_led_init(int gpio)
{
    if (gpio >= 0) {
        const led_strip_config_t strip_cfg = {
            .strip_gpio_num = gpio,
            .max_leds = 1,
            .led_model = LED_MODEL_WS2812,
            .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        };
        const led_strip_rmt_config_t rmt_cfg = {
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10 * 1000 * 1000,
        };
        esp_err_t err = led_strip_new_rmt_device(&strip_cfg, &rmt_cfg, &s_strip);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "no LED on GPIO%d (%s)", gpio, esp_err_to_name(err));
            s_strip = NULL;
        }
    }
    xTaskCreate(led_task, "led", 2560, NULL, 2, NULL);
    return ESP_OK;
}

void status_led_set(led_color_t color, led_mode_t mode)
{
    s_color = color;
    s_mode = mode;
}

void status_led_flash(led_color_t color, int count)
{
    s_flash_color = color;
    s_flash_count = count;
}
