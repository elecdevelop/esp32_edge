#include "usb_cdc.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
/* RTC register that forces the ROM to enter USB download mode on next reset */
#include "soc/rtc_cntl_reg.h"

static const char *TAG = "usb_cdc";

static SemaphoreHandle_t  s_host_ready    = NULL;
static esp_timer_handle_t s_restart_timer = NULL;

/* Timer callback runs outside the TinyUSB task — safe to call esp_restart() */
static void restart_timer_cb(void *arg)
{
    esp_restart();
}

/* ---- magic-byte reset into ROM download mode ----------------------------
 * Sending "ESPBOOT" over the CDC port at any time triggers a reboot into
 * the ROM USB download stub.  The streaming script never sends data, so
 * there is no risk of accidental triggering.
 * ----------------------------------------------------------------------- */
static const char     RESET_MAGIC[]  = "ESPBOOT";
static const uint8_t  RESET_MAGIC_LEN = sizeof(RESET_MAGIC) - 1;  /* 7 */
static uint8_t        s_magic_idx = 0;

static void rx_callback(int itf, cdcacm_event_t *event)
{
    uint8_t buf[64];
    size_t  rx_size = 0;

    if (tinyusb_cdcacm_read(itf, buf, sizeof(buf), &rx_size) != ESP_OK) return;

    for (size_t i = 0; i < rx_size; i++) {
        if (buf[i] == (uint8_t)RESET_MAGIC[s_magic_idx]) {
            if (++s_magic_idx == RESET_MAGIC_LEN) {
                /* Set download-mode flag then fire timer 50 ms later so
                 * esp_restart() runs outside the TinyUSB task context. */
                REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT_M);
                esp_timer_start_once(s_restart_timer, 50 * 1000);
            }
        } else {
            s_magic_idx = 0;
        }
    }
}

/* DTR going high = host opened the port → start streaming */
static void line_state_cb(int itf, cdcacm_event_t *event)
{
    if (event->line_state_changed_data.dtr) {
        xSemaphoreGive(s_host_ready);
    }
}

/* ---- public API --------------------------------------------------------- */

esp_err_t usb_cdc_init(void)
{
    s_host_ready = xSemaphoreCreateBinary();
    if (!s_host_ready) return ESP_ERR_NO_MEM;

    const esp_timer_create_args_t timer_args = {
        .callback = restart_timer_cb,
        .name     = "usb_restart",
    };
    ESP_RETURN_ON_ERROR(esp_timer_create(&timer_args, &s_restart_timer),
                        TAG, "esp_timer_create failed");

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = NULL,
        .string_descriptor        = NULL,
        .external_phy             = false,
        .configuration_descriptor = NULL,
    };
    ESP_RETURN_ON_ERROR(tinyusb_driver_install(&tusb_cfg), TAG,
                        "tinyusb_driver_install failed");

    const tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev                     = TINYUSB_USBDEV_0,
        .cdc_port                    = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz            = 64,
        .callback_rx                 = rx_callback,
        .callback_line_state_changed = line_state_cb,
    };
    ESP_RETURN_ON_ERROR(tusb_cdc_acm_init(&acm_cfg), TAG,
                        "tusb_cdc_acm_init failed");

    ESP_LOGI(TAG, "CDC-ACM ready");
    return ESP_OK;
}

void usb_cdc_wait_for_host(void)
{
    xSemaphoreTake(s_host_ready, portMAX_DELAY);
}

void usb_cdc_write_samples(const int16_t *samples, int n_samples)
{
    const uint8_t *data  = (const uint8_t *)samples;
    int            total = n_samples * (int)sizeof(int16_t);
    int            sent  = 0;

    while (sent < total) {
        size_t n = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0,
                                               data + sent,
                                               total - sent);
        if (n == 0) {
            tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(50));
            vTaskDelay(1);
        } else {
            sent += (int)n;
            if (sent == total) {
                tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, pdMS_TO_TICKS(200));
            }
        }
    }
}
