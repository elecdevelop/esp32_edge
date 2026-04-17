#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "driver/usb_device.h"
#include <string.h>

static const char *TAG = "test";
#define SAMPLE_RATE    16000
#define HOP_SIZE       128
static int16_t s_buf[HOP_SIZE];
static int16_t s_voice_out[HOP_SIZE];
static int16_t s_noise_out[HOP_SIZE];

static void app_task(void *arg)
{
    ESP_LOGI(TAG, "Test task started");
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < HOP_SIZE; j++) {
            s_voice_out[j] = (int16_t)(10000.0f * sin(2.0f*3.14159f*j/128));
            s_noise_out[j] = (int16_t)(5000.0f * sin(2.0f*3.14159f*1.7*j*1782/2834/16000));
        }
        vTaskDelay(pdMS_TO_TICKS(10));
        ESP_LOGI(TAG, "HOP %d completed", i);
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Firmware test started!");
    esp_timer_create_args_t timer_args = {
        .callback = app_task,
        .name = "timer"
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, 100000LL));
}
