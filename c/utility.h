
#ifndef __UTILITY_H
#define __UTILITY_H

#include "redpitaya/rp.h"

// Undecimated samplerate of Red Pitaya
#define RP_BASE_SAMPLERATE 125e6

// Number of samples in ADC buffer, equal to 2**14.
#define RP_BUFFER_SIZE 16384


/**
 * Log-scaled ticks from `vmin` to `vmax` (inclusive) with i from 0 to
 * `imax` (exclusive).
 */
float log_scale_steps(int i, int imax, float vmin, float vmax);


/**
 * Choose largest decimation (longest buffer time) that samples the
 * given frequency with at least 20 points per period.
 */
rp_acq_decimation_t best_decimation_factor(float f, float *samplerate);


/**
 * Acquire complete buffer of both channels.  Triggered immediately
 * after fast input setup (trigger at beginning of buffer).
 */
void acquire_2channels(
        const rp_acq_decimation_t decimation,
        float *buf1, uint32_t *s1,
        float *buf2, uint32_t *s2);

/**
 * Write step function to buffer: 1 before delay time, 0 after.
 * First sample guaranteed to be 1.
 */
void ttl_arb_waveform(
    float samplerate, float delay,
    float *buf, uint32_t bufsize);

#endif // __UTILITY_H
