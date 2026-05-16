#include "dsp_pipeline.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "dsp";

/* Time-domain subtraction tuning knobs
 *   ALPHA : subtraction strength  (1.0 = full noise cancellation)
 *   BETA  : spectral floor factor (prevents negative power → musical noise)
 */
#define ALPHA  1.0f
#define BETA   0.02f

/* ---------- module state ------------------------------------------------ */

/* Previous hop stored for the 50 % overlap frame construction */
static int16_t s_prev_voice[HOP_SIZE];
static int16_t s_prev_noise[HOP_SIZE];

/* ---------- public API --------------------------------------------------- */

esp_err_t dsp_pipeline_init(void)
{
    dsp_pipeline_reset();
    return ESP_OK;
}

void dsp_pipeline_reset(void)
{
    memset(s_prev_voice, 0, sizeof(s_prev_voice));
    memset(s_prev_noise, 0, sizeof(s_prev_noise));
}

void dsp_process_hop(const int16_t *voice_hop,
                     const int16_t *noise_hop,
                     int16_t       *out_hop)
{
    /* ---- Time-domain subtraction: clean = voice - ALPHA * noise ----
     * This works because both channels share the same noise phase,
     * so at the noise frequency (1.7 kHz) the signals are coherent
     * and subtraction cancels perfectly. At the voice frequency (1 kHz)
     * the noise channel is zero, so subtraction does nothing.          */
    for (int i = 0; i < HOP_SIZE; i++) {
        float clean = (float)voice_hop[i] - ALPHA * (float)noise_hop[i];

        /* Clip to int16 range */
        if (clean >  32767.0f) clean =  32767.0f;
        if (clean < -32768.0f) clean = -32768.0f;

        out_hop[i] = (int16_t)clean;
    }

    /* ---- Advance sliding window for next hop ---- */
    memcpy(s_prev_voice, voice_hop, HOP_SIZE * sizeof(int16_t));
    memcpy(s_prev_noise, noise_hop, HOP_SIZE * sizeof(int16_t));
}
