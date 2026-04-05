#include "dsp_pipeline.h"
#include <string.h>
#include <math.h>
#include "esp_dsp.h"
#include "esp_log.h"

static const char *TAG = "dsp";

/* Spectral subtraction tuning knobs
 *   ALPHA : subtraction strength  (1.0 = full noise cancellation)
 *   BETA  : spectral floor factor (prevents negative power → musical noise)
 */
#define ALPHA  1.0f
#define BETA   0.02f

/* ---------- module state ------------------------------------------------ */

/* Periodic Hann window: w[n] = 0.5 * (1 - cos(2π*n/N))
 * With 50 % overlap the windows sum to exactly 1.0 everywhere,
 * giving perfect reconstruction for the unmodified case.           */
static float s_hann[FFT_N];

/* Complex FFT work buffers: interleaved [re0, im0, re1, im1, ...] */
static float s_fft_voice[FFT_N * 2];
static float s_fft_noise[FFT_N * 2];

/* Overlap-add accumulator: second half of previous iFFT frame */
static float s_overlap[HOP_SIZE];

/* Previous hop stored for the 50 % overlap frame construction */
static int16_t s_prev_voice[HOP_SIZE];
static int16_t s_prev_noise[HOP_SIZE];

/* ---------- helpers ------------------------------------------------------ */

/* In-place inverse FFT via conjugate trick:
 *   IFFT(X) = (1/N) * conj( FFT( conj(X) ) )              */
static void do_ifft(float *data, int n)
{
    /* conjugate input */
    for (int i = 0; i < n; i++) data[2 * i + 1] = -data[2 * i + 1];

    dsps_fft2r_fc32(data, n);
    dsps_bit_rev_fc32(data, n);

    /* conjugate and scale output */
    float inv_n = 1.0f / (float)n;
    for (int i = 0; i < n; i++) {
        data[2 * i]     =  data[2 * i]     * inv_n;
        data[2 * i + 1] = -data[2 * i + 1] * inv_n;
    }
}

/* ---------- public API --------------------------------------------------- */

esp_err_t dsp_pipeline_init(void)
{
    for (int n = 0; n < FFT_N; n++) {
        s_hann[n] = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * n / FFT_N));
    }

    dsp_pipeline_reset();

    esp_err_t err = dsps_fft2r_init_fc32(NULL, FFT_N);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "dsps_fft2r_init_fc32 failed: %s", esp_err_to_name(err));
    }
    return err;
}

void dsp_pipeline_reset(void)
{
    memset(s_overlap,    0, sizeof(s_overlap));
    memset(s_prev_voice, 0, sizeof(s_prev_voice));
    memset(s_prev_noise, 0, sizeof(s_prev_noise));
}

void dsp_process_hop(const int16_t *voice_hop,
                     const int16_t *noise_hop,
                     int16_t       *out_hop)
{
    /* ---- Build windowed 256-sample frames: [prev | new] × Hann ---- */
    for (int i = 0; i < HOP_SIZE; i++) {
        s_fft_voice[2 * i]     = (float)s_prev_voice[i] * s_hann[i];
        s_fft_voice[2 * i + 1] = 0.0f;
        s_fft_noise[2 * i]     = (float)s_prev_noise[i] * s_hann[i];
        s_fft_noise[2 * i + 1] = 0.0f;
    }
    for (int i = 0; i < HOP_SIZE; i++) {
        int j = i + HOP_SIZE;
        s_fft_voice[2 * j]     = (float)voice_hop[i] * s_hann[j];
        s_fft_voice[2 * j + 1] = 0.0f;
        s_fft_noise[2 * j]     = (float)noise_hop[i] * s_hann[j];
        s_fft_noise[2 * j + 1] = 0.0f;
    }

    /* ---- Forward FFT on both channels ---- */
    dsps_fft2r_fc32(s_fft_voice, FFT_N);
    dsps_bit_rev_fc32(s_fft_voice, FFT_N);

    dsps_fft2r_fc32(s_fft_noise, FFT_N);
    dsps_bit_rev_fc32(s_fft_noise, FFT_N);

    /* ---- Spectral subtraction (all N bins; conjugate symmetry maintained) ---- */
    for (int k = 0; k < FFT_N; k++) {
        float rv = s_fft_voice[2 * k];
        float iv = s_fft_voice[2 * k + 1];
        float rn = s_fft_noise[2 * k];
        float in = s_fft_noise[2 * k + 1];

        float voice_mag2 = rv * rv + iv * iv;
        float noise_mag2 = rn * rn + in * in;

        float clean_mag2 = voice_mag2 - ALPHA * noise_mag2;
        if (clean_mag2 < BETA * voice_mag2) {
            clean_mag2 = BETA * voice_mag2;
        }

        float scale = (voice_mag2 > 1e-10f) ? sqrtf(clean_mag2 / voice_mag2) : 0.0f;

        s_fft_voice[2 * k]     = rv * scale;
        s_fft_voice[2 * k + 1] = iv * scale;
    }

    /* ---- Inverse FFT ---- */
    do_ifft(s_fft_voice, FFT_N);

    /* ---- Overlap-add: output first HOP_SIZE samples ---- */
    for (int i = 0; i < HOP_SIZE; i++) {
        float val = s_fft_voice[2 * i] + s_overlap[i];
        if (val >  32767.0f) val =  32767.0f;
        if (val < -32768.0f) val = -32768.0f;
        out_hop[i] = (int16_t)val;
    }

    /* ---- Save second half as overlap for next frame ---- */
    for (int i = 0; i < HOP_SIZE; i++) {
        s_overlap[i] = s_fft_voice[2 * (i + HOP_SIZE)];
    }

    /* ---- Advance sliding window ---- */
    memcpy(s_prev_voice, voice_hop, HOP_SIZE * sizeof(int16_t));
    memcpy(s_prev_noise, noise_hop, HOP_SIZE * sizeof(int16_t));
}
