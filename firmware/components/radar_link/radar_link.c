#include "radar_link.h"

#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "radar";

#define RX_BUF_SIZE    4096
#define LINK_STALE_MS  1500
#define BAUD_HUNT_MS   3000

/* 115200 is the usual default; some module firmwares stream faster. */
static const int s_bauds[] = {115200, 1382400, 921600, 460800, 256000};
#define BAUD_COUNT ((int)(sizeof(s_bauds) / sizeof(s_bauds[0])))

static SemaphoreHandle_t s_lock;
static radar_snapshot_t s_snap;
static radar_tf_decoder_t s_decoder;
static radar_wire_event_t s_event; /* large; keep off the task stack */
static int s_uart;
static int64_t s_last_frame_ms; /* any valid frame (drives baud hunting) */
static int64_t s_last_data_ms;  /* last parsed target, cloud or zone frame */

static int64_t now_ms(void)
{
    return esp_timer_get_time() / 1000;
}

static void handle_event(const radar_wire_event_t *ev, int64_t t)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_last_frame_ms = t;
    s_snap.frames++;
    if (ev->mode != RADAR_WIRE_UNKNOWN) {
        s_snap.mode = ev->mode;
    }
    if (ev->type == RADAR_TF_TYPE_TARGET && ev->has_target_sample) {
        s_snap.target_ms = t;
        s_last_data_ms = t;
        s_snap.target_count = ev->sample.target_count;
        s_snap.blip_count = ev->sample.blip_count;
        memcpy(s_snap.blips, ev->sample.blips, sizeof(s_snap.blips));
    } else if (ev->type == RADAR_TF_TYPE_CLOUD && ev->has_cloud) {
        s_snap.cloud_ms = t;
        s_last_data_ms = t;
        s_snap.cloud_count = ev->cloud_count;
        memcpy(s_snap.cloud, ev->cloud, sizeof(radar_wire_point_t) * (size_t)ev->cloud_count);
    } else if (ev->type == RADAR_TF_TYPE_ZONES && ev->has_zones) {
        s_snap.zones_ms = t;
        s_last_data_ms = t;
        memcpy(s_snap.zone_flags, ev->zone_flags, sizeof(s_snap.zone_flags));
    }
    xSemaphoreGive(s_lock);
}

static void radar_task(void *arg)
{
    (void)arg;
    uint8_t buf[256];
    int baud_idx = 0;
    int64_t baud_since = now_ms();

    for (;;) {
        const int n = uart_read_bytes(s_uart, buf, sizeof(buf), pdMS_TO_TICKS(50));
        const int64_t t = now_ms();
        for (int i = 0; i < n; ++i) {
            if (radar_tf_decoder_feed(&s_decoder, buf[i], &s_event)) {
                handle_event(&s_event, t);
            }
        }
        if (n > 0) {
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_snap.rx_bytes += (uint32_t)n;
            xSemaphoreGive(s_lock);
        }

        /* No valid frame for a while: try the next baud rate. */
        const int64_t quiet_since = s_last_frame_ms > baud_since ? s_last_frame_ms : baud_since;
        if (t - quiet_since > BAUD_HUNT_MS) {
            baud_idx = (baud_idx + 1) % BAUD_COUNT;
            uart_set_baudrate(s_uart, s_bauds[baud_idx]);
            uart_flush_input(s_uart);
            radar_tf_decoder_init(&s_decoder);
            baud_since = t;
            xSemaphoreTake(s_lock, portMAX_DELAY);
            s_snap.baud = s_bauds[baud_idx];
            xSemaphoreGive(s_lock);
            ESP_LOGW(TAG, "no frames, trying %d baud", s_bauds[baud_idx]);
        }
    }
}

esp_err_t radar_link_init(int uart_num, int tx_gpio, int rx_gpio, int rst_gpio)
{
    s_lock = xSemaphoreCreateMutex();
    ESP_RETURN_ON_FALSE(s_lock, ESP_ERR_NO_MEM, TAG, "mutex");
    s_uart = uart_num;
    radar_tf_decoder_init(&s_decoder);
    memset(&s_snap, 0, sizeof(s_snap));
    s_snap.baud = s_bauds[0];

    if (rst_gpio >= 0) {
        /* Hold the radar out of reset (RST is active low). */
        const gpio_config_t io = {
            .pin_bit_mask = 1ULL << rst_gpio,
            .mode = GPIO_MODE_OUTPUT,
        };
        ESP_RETURN_ON_ERROR(gpio_config(&io), TAG, "rst gpio");
        gpio_set_level(rst_gpio, 0);
        vTaskDelay(pdMS_TO_TICKS(20));
        gpio_set_level(rst_gpio, 1);
    }

    const uart_config_t cfg = {
        .baud_rate = s_bauds[0],
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_RETURN_ON_ERROR(uart_driver_install(uart_num, RX_BUF_SIZE, 0, 0, NULL, 0), TAG, "install");
    ESP_RETURN_ON_ERROR(uart_param_config(uart_num, &cfg), TAG, "config");
    ESP_RETURN_ON_ERROR(uart_set_pin(uart_num, tx_gpio, rx_gpio, UART_PIN_NO_CHANGE,
                                     UART_PIN_NO_CHANGE), TAG, "pins");

    BaseType_t ok = xTaskCreate(radar_task, "radar", 4096, NULL, 6, NULL);
    ESP_RETURN_ON_FALSE(ok == pdPASS, ESP_ERR_NO_MEM, TAG, "task");
    ESP_LOGI(TAG, "UART%d TX=GPIO%d RX=GPIO%d", uart_num, tx_gpio, rx_gpio);
    return ESP_OK;
}

void radar_link_snapshot(radar_snapshot_t *out)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    *out = s_snap;
    const int64_t last = s_last_data_ms;
    xSemaphoreGive(s_lock);
    /* Only frames we could actually decode count: other frame types or an
     * unknown layout must not look like a working radar. */
    out->link_ok = last != 0 && now_ms() - last < LINK_STALE_MS;
}
