
#ifndef __DEMODULATION_H
#define __DEMODULATION_H

#include <unistd.h>
#include <stdint.h>


/**
 * Use IQ demodulation to get amplitude and phase of component with
 * frequency f.
 *
 * The signal should be similar to $A \cos(2\pi f t) + \phi$.
 * This modulation function internally works with in units of samples,
 * not time.  The result is the same.
 *
 * @param signal Array with input signal.
 * @param n Number of samples.
 * @param f Frequency to isolate [Hz].
 * @param samplerate Samplerate of signal [samples / s].
 * @param A Result for amplitude in same units as input signal.
 * @param phi Result for phase in rad.
 */
void demodulate(
    const float *signal, const size_t n,
    const float f, const float samplerate,
    float *A, float *phi);


#endif // __DEMODULATION_H
