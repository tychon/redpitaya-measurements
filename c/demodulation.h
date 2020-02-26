
#ifndef __DEMODULATION_H
#define __DEMODULATION_H

#include <unistd.h>
#include <stdint.h>


/**
 * Calculate average (arithmetic mean) of data.
 */
float mean(const float *data, const size_t n);


/**
 * Use IQ demodulation to get amplitude and phase of component with
 * frequency f.
 *
 * The signal should be similar to $A \cos(2\pi f t + \phi)$.
 * This modulation function internally works with in units of samples,
 * not time.  The result is the same.
 *
 * Remove DC offset before applying this function.
 *
 * @param signal Array with input signal.
 * @param n Number of samples.
 * @param f Frequency to isolate [Hz].
 * @param samplerate Samplerate of signal [samples / s].
 * @param A Result for amplitude in same units as input signal.
 * @param phi Result for phase in rad.
 * @param offset Result for DC offset in same units as input signal.
 */
void demodulate(
    const float *signal, const size_t n,
    const float f, const float samplerate,
    float *A, float *phi, float *offset);


/**
 * Calculate standard deviation of reconstruction from signal.
 *
 * @param signal Array with input signal.
 * @param n Number of samples.
 * @param samplerate Samplerate of signal [samples / s].
 * @param freq Frequency [Hz].
 * @param amplitude Amplitude of reconstructed signal.
 * @param phase Phase of reconstructed signal in rad.
 * @param offset DC offset of signal
 */
float deviation_from_reconstruction(
    const float *signal, const size_t n, const float samplerate, const float freq,
    const float amplitude, const float phase, const float offset);


#endif // __DEMODULATION_H
