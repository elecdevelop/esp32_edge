#pragma once
#include <stdint.h>

/* Synthesise one hop of test signals.
 *
 * noise_buf  : 1.7 kHz square wave  (noise-reference mic)
 * voice_buf  : 1 kHz sine + 1.7 kHz square wave  (voice mic)
 * n_samples  : number of samples to generate (must be HOP_SIZE)
 *
 * Both channels share the same square-wave phase so the noise
 * source is coherent between mics, as it would be in the real setup.
 * Amplitudes are scaled to 50 % of int16 full-scale to prevent clipping
 * after mixing.
 */
void signal_gen_next_hop(int16_t *noise_buf, int16_t *voice_buf, int n_samples);
