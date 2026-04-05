#include "usb_cdc.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/usb_serial_jtag.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "usb_cdc";

#define TX_BUF_SIZE  4096
#define RX_BUF_SIZE  256

esp_err_t usb_cdc_init(void)
{
    usb_serial_jtag_driver_config_t cfg = {
        .rx_buffer_size = RX_BUF_SIZE,
        .tx_buffer_size = TX_BUF_SIZE,
    };
    ESP_RETURN_ON_ERROR(usb_serial_jtag_driver_install(&cfg),
                        TAG, "usb_serial_jtag_driver_install failed");
    ESP_LOGI(TAG, "USB Serial/JTAG ready");
    return ESP_OK;
}

/* Block until the host sends any byte (used as a "start" signal). */
void usb_cdc_wait_for_host(void)
{
    uint8_t dummy;
    while (usb_serial_jtag_read_bytes(&dummy, 1, pdMS_TO_TICKS(100)) == 0) {
        vTaskDelay(1);
    }
}

void usb_cdc_write_samples(const int16_t *samples, int n_samples)
{
    const uint8_t *data  = (const uint8_t *)samples;
    int            total = n_samples * (int)sizeof(int16_t);
    int            sent  = 0;

    while (sent < total) {
        int n = usb_serial_jtag_write_bytes(data + sent,
                                             total - sent,
                                             pdMS_TO_TICKS(200));
        if (n > 0) sent += n;
    }
}
