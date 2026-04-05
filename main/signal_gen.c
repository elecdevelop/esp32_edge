#include "signal_gen.h"
#include <math.h>

#define SAMPLE_RATE   16000.0f
#define SINE_FREQ     1000.0f
#define NOISE_FREQ    1700.0f
#define SINE_AMP      6500.0f   /* voice sine amplitude */
#define SQUARE_AMP    4000.0f   /* noise square amplitude — matched in both channels */

static float s_sine_phase   = 0.0f;
static float s_square_phase = 0.0f;

static const float SINE_INC   = 2.0f * (float)M_PI * SINE_FREQ  / SAMPLE_RATE;
static const float SQUARE_INC = 2.0f * (float)M_PI * NOISE_FREQ / SAMPLE_RATE;

void signal_gen_next_hop(int16_t *noise_buf, int16_t *voice_buf, int n_samples)
{
    for (int i = 0; i < n_samples; i++) {
        float sq = (sinf(s_square_phase) >= 0.0f) ? SQUARE_AMP : -SQUARE_AMP;
        float si = SINE_AMP * sinf(s_sine_phase);

        noise_buf[i] = (int16_t)sq;
        /* Noise component in voice matches noise reference amplitude exactly,
         * so ALPHA=1.0 spectral subtraction is correctly calibrated. Peak = 10500. */
        voice_buf[i] = (int16_t)(si + sq);

        s_square_phase += SQUARE_INC;
        if (s_square_phase >= 2.0f * (float)M_PI) s_square_phase -= 2.0f * (float)M_PI;

        s_sine_phase += SINE_INC;
        if (s_sine_phase >= 2.0f * (float)M_PI) s_sine_phase -= 2.0f * (float)M_PI;
    }
}
