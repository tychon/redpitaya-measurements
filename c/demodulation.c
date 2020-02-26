
#include <stdlib.h>
#include <math.h>

#include "demodulation.h"


float mean(const float *buf, const size_t n) {
    float m = 0;
    for (size_t i = 0; i < n; i++)
        m += buf[i];
    return m / n;
}


/**
 * Integrate equally spaced samples using the trapezoidal rule.
 *
 * Trapezoidal rule: sum(dx * (buf[i] + buf[i+1]) / 2)
 * = dx / 2 * (buf[0] + 2 * buf[1] + ... + 2 * buf[n-2] + buf[n-1])
 */
float integrate_trapezoidal(const float *buf, const size_t n, const float dx) {
    if (n == 0) return 0;
    else if (n == 1) {
        return buf[0] * dx;
    } else {
        float ret = 0;
        ret += buf[0];
        for (size_t i = 1; i < (n-1); i++) ret += 2 * buf[i];
        ret += buf[n-1];
        return ret * dx / 2.0;
    }
}


/**
 * Integrate signal after multiplication with harmonic signal.
 *
 * Trapezoidal rule: sum(dx * (buf[i] + buf[i+1]) / 2)
 * = dx / 2 * (buf[0] + 2 * buf[1] + ... + 2 * buf[n-2] + buf[n-1])
 *
 * @param buf Input data points.
 * @param n Number of data points.
 * @param dx Spacing of data points.
 * @param f Frequency of modulation [per sample]
 * @param phase Phase of modulation [rad].
 */
float integrate_modulated_trapezoidal(
        const float *buf, const size_t n, const float dx,
        const float f, const float phase) {
    if (n == 0) return 0;
    else if (n == 1) {
        return buf[0] * dx * cos(phase);
    } else {
        float ret = 0;
        ret += buf[0] * cos(phase);
        for (size_t i = 1; i < (n-1); i++)
            ret += 2 * buf[i] * cos(2*M_PI * f * i + phase);
        ret += buf[n-1] * cos(2*M_PI * f * (n-1) + phase);
        return ret * dx / 2.0;
    }
}


void demodulate(
        const float *signal, const size_t n,
        const float f, const float samplerate,
        float *A, float *phi, float *offset) {
    // truncate to complete periods
    uint32_t nsamples = floor((float)n * f / samplerate) * samplerate / f;
    float dc = *offset = mean(signal, nsamples);

    float *tmpsignal = (float *)malloc(nsamples * sizeof(float));
    for (uint32_t i = 0; i < nsamples; i++)
        tmpsignal[i] = signal[i] - dc;

    float I = integrate_modulated_trapezoidal(
        tmpsignal, nsamples, 1.0, f/samplerate, 0.0);
    float Q = integrate_modulated_trapezoidal(
        tmpsignal, nsamples, 1.0, f/samplerate, -M_PI/2.0);
    *A = sqrt(I*I + Q*Q) * 2.0 / nsamples;
    *phi = atan2(-Q, I);
}


float deviation_from_reconstruction(
        const float *signal, const size_t n, const float samplerate, const float freq,
        const float amplitude, const float phase, const float offset) {
    float d, s = 0;
    for (size_t i = 0; i < n; i++) {
        d = signal[i] - offset - amplitude * cos(2*M_PI * freq * i / samplerate + phase);
        s += d*d;
    }
    return sqrt(s / n);
}
