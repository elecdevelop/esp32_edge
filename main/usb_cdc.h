#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/* Initialise TinyUSB CDC-ACM device and install the USB driver. */
esp_err_t usb_cdc_init(void);

/* Block until the host opens the serial port (DTR asserted).
 * Safe to call repeatedly — each call waits for a fresh DTR rising edge. */
void usb_cdc_wait_for_host(void);

/* Write n_samples of int16 PCM, blocking until all bytes are sent.
 * Yields to the TinyUSB task if the TX buffer is momentarily full. */
void usb_cdc_write_samples(const int16_t *samples, int n_samples);
