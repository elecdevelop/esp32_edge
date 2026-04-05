#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "signal_gen.h"
#include "dsp_pipeline.h"
#include "usb_cdc.h"

static const char *TAG = "main";

#define SAMPLE_RATE    16000
#define RECORD_SECONDS 5
#define TOTAL_SAMPLES  (SAMPLE_RATE * RECORD_SECONDS)
#define TOTAL_HOPS     (TOTAL_SAMPLES / HOP_SIZE)

/* 128 samples at 16 kHz = 8 000 µs per hop */
#define HOP_PERIOD_US  (HOP_SIZE * 1000000LL / SAMPLE_RATE)

void app_main(void)
{
    ESP_ERROR_CHECK(dsp_pipeline_init());
    ESP_ERROR_CHECK(usb_cdc_init());

    static int16_t noise_hop[HOP_SIZE];
    static int16_t voice_hop[HOP_SIZE];
    static int16_t out_hop[HOP_SIZE];

    while (true) {
        /* Wait for host to open the serial port (DTR high) */
        ESP_LOGI(TAG, "Waiting for host...");
        usb_cdc_wait_for_host();

        dsp_pipeline_reset();

        ESP_LOGI(TAG, "Streaming %d samples (%ds @ %dHz)...",
                 TOTAL_SAMPLES, RECORD_SECONDS, SAMPLE_RATE);

        int64_t next_hop_time = esp_timer_get_time();

        for (int hop = 0; hop < TOTAL_HOPS; hop++) {
            signal_gen_next_hop(noise_hop, voice_hop, HOP_SIZE);
            dsp_process_hop(voice_hop, noise_hop, out_hop);
            usb_cdc_write_samples(out_hop, HOP_SIZE);

            next_hop_time += HOP_PERIOD_US;
            int64_t slack_us = next_hop_time - esp_timer_get_time();
            if (slack_us > 1000) {
                vTaskDelay(pdMS_TO_TICKS(slack_us / 1000));
            }
        }

        ESP_LOGI(TAG, "Stream complete — ready for next capture");
    }
}
