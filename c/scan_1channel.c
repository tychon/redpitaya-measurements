/**
 * Run one-variable frequency scan for RF outputs and response at RF
 * inputs at the same frequency and twice the frequency.
 *
 * Decimation factor for sampling rate is chosen such that the
 * waveform is sampled by at least 20 samples per period.
 *
 * Usage: ./run.sh IP scan_1channel.x F_START,STEPS,F_END [full]
 *
 * Where start and end frequencies F_START and F_END are floats in
 * units of Hertz, and STEPS is an integer (steps between start and
 * end frequency on log scale).  Instead of range also a single number
 * can be supplied.  Use optional flag `full` to output
 * complete ADC buffers instead of demodulated data.
 *
 * Prints demodulated data to stdout in 13 columns
 * (whitespaces / column separators are tabs):
 *
 *     f samplerate A1 A2 A12 A22 ph2 ph12 ph22 dc1 dc2 dc12 dc22 err1 err2 err12 err22
 *
 * Numbers indicate: 1 for CH 1, 2 for CH 2, 12 for CH2-CH1,
 * 22 for CH2 at double frequency.
 * The global phase is not well defined and ph2 is (phase2-phase1)
 * ph22 is (phase22-phase1*2). dc1, dc2, and dc22 are the DC offsets
 * and err the reconstruction errors (sqrt of mean of squares of deviation) respectively.
 * Output data comes with header.
 *
 * With flag `full` outputs
 *
 *     f samplerate 1 v0 v1 v2 v3 v4 v5 ...
 *     f samplerate 2 v0 v1 v2 v3 v4 ...
 *
 * Output data comes without header.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include "rp.h"

#include "demodulation.h"
#include "utility.h"


#define HIGH_PASS_FILTER_SETTLING_TIME 10e3


int main(int argc, char **argv) {
    // Parse arguments
    if (argc < 2 || argc > 3) {
        exit(1);
    }
    float fstart, fend;
    int nsteps;
    if (!parse_cmd_line_range(argv[1], &fstart, &fend, &nsteps)) {
        fprintf(stderr, "Invalid argument.\n");
        exit(1);
    }
    bool fulldata = argc == 3;

    // Initialize IO.
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }
    // Initialize outputs
    rp_GenReset();
    rp_GenFreq(RP_CH_1, fstart);
    rp_GenAmp(RP_CH_1, 1);
    rp_GenOffset(RP_CH_1, 0);
    rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
    rp_GenMode(RP_CH_1, RP_GEN_MODE_CONTINUOUS);
    // Trigger setup only needed in burst mode.
    // Actually, setting triggers overwrites the mode.

    // Allocate data buffers
    float *buf1 = (float*)malloc(RP_BUFFER_SIZE * sizeof(float));
    float *buf2 = (float*)malloc(RP_BUFFER_SIZE * sizeof(float));
    float *buf12 = (float*)malloc(RP_BUFFER_SIZE * sizeof(float));

    if (! fulldata)
        printf("f\tsamplerate\tA1\tA2\tA12\tA22\tph2\tph12\tph22\tdc1\tdc2\tdc12\tdc22\terr1\terr2\terr12\terr22\n");

    // Scan
    float samplerate;
    for (int i = 0; i < nsteps; i++) {
        float f = log_scale_steps(i, nsteps, fstart, fend);
        rp_acq_decimation_t dec = best_decimation_factor(f, &samplerate);

        fprintf(stderr, "%3.0f%%  %6.1fkHz  ", 100.0*i/(nsteps-1), f/1e3);

        rp_GenFreq(RP_CH_1, f);
        rp_GenOutEnable(RP_CH_1);
        // wait for high-pass filter to settle
        usleep(HIGH_PASS_FILTER_SETTLING_TIME);

        uint32_t s1 = RP_BUFFER_SIZE, s2 = RP_BUFFER_SIZE;
        acquire_2channels(dec, buf1, &s1, buf2, &s2);
        rp_GenOutDisable(RP_CH_1);

        for (uint32_t j = 0; j < RP_BUFFER_SIZE; j++) {
            buf12[j] = buf2[j] - buf1[j];
        }

        if (! fulldata) {
            float A1, A2, A12, A22;
            float phase1, phase2, phase12, phase22, ph2, ph12, ph22;
            float offset1, offset2, offset12, offset22;
            float sd1, sd2, sd12, sd22;
            demodulate(buf1, s1, f, samplerate, &A1, &phase1, &offset1);
            demodulate(buf2, s2, f, samplerate, &A2, &phase2, &offset2);
            demodulate(buf2, s2, 2*f, samplerate, &A22, &phase22, &offset22);
            demodulate(buf12, (s1 < s2)? s1 : s2, f, samplerate, &A12, &phase12, &offset12);
            sd1  = deviation_from_reconstruction(buf1, s1, samplerate, f, A1, phase1, offset1);
            sd2  = deviation_from_reconstruction(buf2, s2, samplerate, f, A2, phase2, offset2);
            sd22 = deviation_from_reconstruction(buf2, s2, samplerate, 2*f, A22, phase22, offset22);
            sd12  = deviation_from_reconstruction(buf12, (s1 < s2)? s1 : s2, samplerate, f, A12, phase12, offset12);
            // phase difference of CH2 - CH1 in range [-pi, pi]
            ph2 = fmod(phase2 - phase1 + M_PI, 2*M_PI) - M_PI;
            ph12 = fmod(phase12 - phase1 + M_PI, 2*M_PI) - M_PI;
            // phase difference of CH1 and CH2 @ double frequency in range [-pi to pi]
            ph22 = fmod(phase22 - fmod(phase1*2, 2*M_PI) + M_PI, 2*M_PI) - M_PI;
            printf("%e\t%f\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\t%e\n",
                   f, samplerate, A1, A2, A12, A22,
                   ph2, ph12, ph22,
                   offset1, offset2, offset12, offset22,
                   sd1, sd2, sd12, sd22);

            fprintf(stderr, "%5.1f mV  %5.1f mV  %5.1f mV  %5.1f mV\n",
                    1e3*A1, 1e3*A2, 1e3*A12, 1e3*A22);
        } else {
            printf("%f\t%f\t1", f, samplerate);
            for (uint32_t k = 0; k < s1; k++)
                printf("\t%f", buf1[k]);
            printf("\n%f\t%f\t2", f, samplerate);
            for (uint32_t k = 0; k < s2; k++)
                printf("\t%f", buf2[k]);
            printf("\n");

            fprintf(stderr, "\n");
        }
    }


    free(buf1);
    free(buf2);
    free(buf12);
    rp_GenReset();
    rp_Release();
    return 0;
}
