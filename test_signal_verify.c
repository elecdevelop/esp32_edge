#include <stdio.h>
#include <math.h>

#define SAMPLE_RATE    16000.0f
#define VOICE_FREQ     1000.0f
#define NOISE_FREQ     1700.0f
#define VOICE_AMP      10000.0f
#define NOISE_AMP      5000.0f
#define N_SAMPLES      128

static const float VOICE_INC = 2.0f * (float)M_PI * VOICE_FREQ / SAMPLE_RATE;
static const float NOISE_INC = 2.0f * (float)M_PI * NOISE_FREQ / SAMPLE_RATE;

void signal_gen_next_hop(float *noise_buf, float *voice_buf, int n_samples)
{
    float s_voice_phase = 0.0f;
    float s_noise_phase = 0.0f;

    for (int i = 0; i < n_samples; i++) {
        noise_buf[i] = NOISE_AMP * sinf(s_noise_phase);
        voice_buf[i] = VOICE_AMP * sinf(s_voice_phase) + NOISE_AMP * sinf(s_noise_phase);

        s_voice_phase += VOICE_INC;
        if (s_voice_phase >= 2.0f * (float)M_PI) s_voice_phase -= 2.0f * (float)M_PI;

        s_noise_phase += NOISE_INC;
        if (s_noise_phase >= 2.0f * (float)M_PI) s_noise_phase -= 2.0f * (float)M_PI;
    }
}

void print_signal(float *buf, const char *name, int length)
{
    printf("%s Signal (first 10 samples):\n", name);
    for (int i = 0; i < length && i < 10; i++) {
        printf("  [%d] = %f\n", i, buf[i]);
    }
}

void analyze_signal(float *buf, int length, float freq)
{
    float sum_sq = 0.0f;
    float min_val = buf[0], max_val = buf[0];

    for (int i = 0; i < length; i++) {
        sum_sq += buf[i] * buf[i];
        if (buf[i] < min_val) min_val = buf[i];
        if (buf[i] > max_val) max_val = buf[i];
    }

    printf("%s Analysis:\n", freq == 1000.0f ? "Voice (1kHz)" : "Noise (1.7kHz)");
    printf("  RMS = %.1f\n", sqrtf(sum_sq / length));
    printf("  Min = %.1f (max possible: %f)\n", min_val, -VOICE_AMP);
    printf("  Max = %.1f (max possible: %f)\n", max_val, VOICE_AMP);
}

int main(void)
{
    float noise_buf[N_SAMPLES];
    float voice_buf[N_SAMPLES];

    signal_gen_next_hop(noise_buf, voice_buf, N_SAMPLES);

    printf("============================================================\n");
    printf("Signal Generation Test\n");
    printf("============================================================\n\n");

    print_signal(noise_buf, "Noise", N_SAMPLES);
    print_signal(voice_buf, "Voice (mixed with noise)", N_SAMPLES);
    printf("\n");

    analyze_signal(noise_buf, N_SAMPLES, NOISE_FREQ);
    analyze_signal(voice_buf, N_SAMPLES, VOICE_FREQ);

    printf("\nExpected: voice_buf contains 1kHz tone at ±10000 plus 1.7kHz noise at ±5000\n");
    printf("Expected: noise_buf contains only 1.7kHz tone at ±5000\n");

    return 0;
}
