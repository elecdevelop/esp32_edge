#include "signal_gen.h"
#include <math.h>

#define SAMPLE_RATE  16000.0f
#define SINE_FREQ    1000.0f
#define NOISE_FREQ   1700.0f
#define AMPLITUDE    16000.0f   /* 50 % of INT16_MAX — leaves headroom for mixing */

static float s_sine_phase   = 0.0f;
static float s_square_phase = 0.0f;

static const float SINE_INC   = 2.0f * (float)M_PI * SINE_FREQ  / SAMPLE_RATE;
static const float SQUARE_INC = 2.0f * (float)M_PI * NOISE_FREQ / SAMPLE_RATE;

void signal_gen_next_hop(int16_t *noise_buf, int16_t *voice_buf, int n_samples)
{
    for (int i = 0; i < n_samples; i++) {
        float sq = (sinf(s_square_phase) >= 0.0f) ? AMPLITUDE : -AMPLITUDE;
        float si = AMPLITUDE * sinf(s_sine_phase);

        noise_buf[i] = (int16_t)sq;
        /* Mix 50/50 so the sum stays within int16 range */
        voice_buf[i] = (int16_t)((si + sq) * 0.5f);

        s_square_phase += SQUARE_INC;
        if (s_square_phase >= 2.0f * (float)M_PI) s_square_phase -= 2.0f * (float)M_PI;

        s_sine_phase += SINE_INC;
        if (s_sine_phase >= 2.0f * (float)M_PI) s_sine_phase -= 2.0f * (float)M_PI;
    }
}
