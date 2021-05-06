/**
 * Drive the two RF outputs at the same frequency and measure both RF
 * analog inputs at 125Msps/8.  Print as columns to stdout and
 * demodulation data to stderr.
 *
 * Supply frequency as command line argument.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rp.h"

#include "demodulation.h"


int main(int argc, char **argv){
    /* Parse argument */
    if (argc != 2) {
        exit(1);
    }
    float freq = atof(argv[1]);

    /* Print error, if rp_Init() function failed */
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    // Setup both generator channels
    rp_GenReset();
    rp_GenFreq(RP_CH_1, freq);
    rp_GenFreq(RP_CH_2, freq);
    rp_GenAmp(RP_CH_1, 0.5);
    rp_GenAmp(RP_CH_2, 0.5);
    rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
    rp_GenWaveform(RP_CH_2, RP_WAVEFORM_SINE);
    rp_GenMode(RP_CH_1, RP_GEN_MODE_CONTINUOUS);
    rp_GenMode(RP_CH_2, RP_GEN_MODE_CONTINUOUS);
    // Trigger setup only needed in burst mode.
    // Actually, setting triggers overwrites the mode.

    // Setup both ADC channels
    rp_AcqReset();
    rp_AcqSetGain(RP_CH_1, RP_LOW);
    rp_AcqSetGain(RP_CH_2, RP_LOW);
    rp_AcqSetDecimation(RP_DEC_8);
    rp_AcqSetTriggerDelay(8192);
    rp_AcqSetAveraging(1);

    float samplerate;
    rp_AcqGetSamplingRateHz(&samplerate);

    rp_AcqStart(); // start acquiring
    rp_GenOutEnable(RP_CH_1);
    rp_GenOutEnable(RP_CH_2);

    // time frame of one buffer in us
    const uint32_t maxbufsize = 16384;
    uint32_t buffertime = maxbufsize * 1e6 / samplerate;
    // Wait for "look ahead" buffer to fill up
    usleep(buffertime);
    // and wait for output to settle with high-capacitance high pass
    // RC filter.
    usleep(1e4); // 10ms

    // trigger and wait for full buffer
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);
    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
    while (1) {
        rp_AcqGetTriggerState(&state);
        if(state == RP_TRIG_STATE_TRIGGERED){
            break;
        }
    }
    usleep(buffertime);

    // Retrieve data
    uint32_t bufsize1 = maxbufsize;
    float *buf1 = (float *)malloc(maxbufsize * sizeof(float));
    rp_AcqGetOldestDataV(RP_CH_1, &bufsize1, buf1);

    uint32_t bufsize2 = maxbufsize;
    float *buf2 = (float *)malloc(maxbufsize * sizeof(float));
    rp_AcqGetOldestDataV(RP_CH_2, &bufsize2, buf2);

    rp_GenReset();
    rp_AcqReset();

    // Print data to stdout
    for(uint32_t i = 0; i < bufsize1 && i < bufsize2; i++){
        printf("%f\t%f\n", buf1[i], buf2[i]);
    }

    // Demodulate and print info to stderr
    float A, phase, offset, sd;
    demodulate(buf1, bufsize1, freq, samplerate, &A, &phase, &offset);
    sd = deviation_from_reconstruction(buf1, bufsize1, samplerate, freq, A, phase, offset);
    fprintf(stderr, "1: A = %f V,  phase = %f rad,  offset = %f V,  sd = %.2e (%.2f%%)\n", A, phase, offset, sd, 100*sd/A);

    demodulate(buf2, bufsize2, freq, samplerate, &A, &phase, &offset);
    sd = deviation_from_reconstruction(buf2, bufsize2, samplerate, freq, A, phase, offset);
    fprintf(stderr, "2: A = %f V,  phase = %f rad,  offset = %f V,  sd = %.2e (%.2f%%)  @ f = %.1e Hz\n",
            A, phase, offset, sd, 100*sd/A, freq);

    demodulate(buf2, bufsize2, 2*freq, samplerate, &A, &phase, &offset);
    sd = deviation_from_reconstruction(buf2, bufsize2, samplerate, 2*freq, A, phase, offset);
    fprintf(stderr, "2: A = %f V,  phase = %f rad,  offset = %f V,  sd = %.2e (%.2f%%)  @ 2f = %.1e Hz\n",
            A, phase, offset, sd, 100*sd/A, 2*freq);

    free(buf1);
    free(buf2);
    rp_Release();
    return 0;
}
