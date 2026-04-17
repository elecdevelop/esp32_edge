#include "signal_gen.h"
#include <math.h>

#define SAMPLE_RATE    16000.0f
#define VOICE_FREQ     1000.0f
#define NOISE_FREQ     1700.0f
#define VOICE_AMP      10000.0f  /* voice sine ±10000 */
#define NOISE_AMP      5000.0f   /* noise sine ±5000  */

static float s_voice_phase = 0.0f;
static float s_noise_phase = 0.0f;

static const float VOICE_INC = 2.0f * (float)M_PI * VOICE_FREQ / SAMPLE_RATE;
static const float NOISE_INC = 2.0f * (float)M_PI * NOISE_FREQ / SAMPLE_RATE;

/* Test 2: noise mic = 1.7 kHz sine ±5000, voice mic = 1 kHz sine ±10000.
 * The voice mic has NO noise component — noise reference is a separate tone.
 * Expected output: 1 kHz sine preserved, no 1.7 kHz in output. */
void signal_gen_next_hop(int16_t *noise_buf, int16_t *voice_buf, int n_samples)
{
    for (int i = 0; i < n_samples; i++) {
        noise_buf[i] = (int16_t)(NOISE_AMP * sinf(s_noise_phase));
        voice_buf[i] = (int16_t)(VOICE_AMP * sinf(s_voice_phase) + NOISE_AMP * sinf(s_noise_phase));

        s_voice_phase += VOICE_INC;
        if (s_voice_phase >= 2.0f * (float)M_PI) s_voice_phase -= 2.0f * (float)M_PI;

        s_noise_phase += NOISE_INC;
        if (s_noise_phase >= 2.0f * (float)M_PI) s_noise_phase -= 2.0f * (float)M_PI;
    }
}
