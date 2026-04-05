#pragma once
#include <stdint.h>
#include "esp_err.h"

#define FFT_N    256   /* FFT frame length (samples) */
#define HOP_SIZE 128   /* Hop size for 50 % overlap  */

/* Initialise FFT tables.  Call once at startup. */
esp_err_t dsp_pipeline_init(void);

/* Reset overlap and history buffers for a clean start on a new capture. */
void dsp_pipeline_reset(void);

/* Process one hop of HOP_SIZE samples from each synthesised channel.
 *
 * voice_hop  : voice channel (1 kHz sine + 1.7 kHz square)
 * noise_hop  : noise channel (1.7 kHz square)
 * out_hop    : de-noised output, HOP_SIZE int16 samples
 *
 * Internally maintains a 256-sample sliding window (50 % overlap).
 * First call produces valid output; initial overlap buffer is zero.
 */
void dsp_process_hop(const int16_t *voice_hop,
                     const int16_t *noise_hop,
                     int16_t       *out_hop);
