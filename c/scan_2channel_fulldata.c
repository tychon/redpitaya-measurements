/**
 * Run two-variable frequency scan for RF outputs (scan both
 * frequencies independently) and measure one response at RF IN 1.
 * Connect RF IN 2 to RF OUT 1 for phase reference if needed.
 *
 * Set RF IN 1 to high voltage (+- 20V) to allow for resonance peaks!
 *
 * Sampling at 125Msps/8.
 *
 * Prints both ADC buffers to stdout in two lines per frequency data
 * point:
 *
 *     f1 f2 1 v0 v1 v2 v3 v4 v5 ...
 *     f1 f2 2 v0 v1 v2 v3 v4 ...
 *
 * Supply three command line arguments:
 * start frequency [kHz], end frequency [kHz], steps per channel [kHz]
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "redpitaya/rp.h"


// Number of samples in ADC buffer, equal to 2**14.
#define RP_BUFFER_SIZE 16384

// 125Msps / 8 = 15.6Msps
#define DECIMATED_SAMPLERATE 15625000


void acquire(
        float f1, float f2,
        float *buf1, uint32_t *s1,
        float *buf2, uint32_t *s2) {
    rp_GenFreq(RP_CH_1, f1);
    rp_GenFreq(RP_CH_2, f2);
    rp_GenOutEnable(RP_CH_1);
    rp_GenOutEnable(RP_CH_2);

    rp_AcqSetGain(RP_CH_1, RP_HIGH);
    rp_AcqSetGain(RP_CH_2, RP_HIGH);
    rp_AcqSetDecimation(RP_DEC_8);
    rp_AcqSetTriggerDelay(8192);
    rp_AcqSetAveraging(1);
    rp_AcqStart();

    // time frame of one buffer in us
    uint32_t buffertime = RP_BUFFER_SIZE * 8 / 125;
    // wait for "look ahead" buffer to fill up
    usleep(buffertime);
    // wait for trigger
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);
    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
    while (1) {
        rp_AcqGetTriggerState(&state);
        if(state == RP_TRIG_STATE_TRIGGERED){
            break;
        }
    }
    usleep(buffertime);

    rp_GenOutDisable(RP_CH_1);
    rp_GenOutDisable(RP_CH_2);

    // Retrieve data
    rp_AcqGetOldestDataV(RP_CH_1, s1, buf1);
    rp_AcqGetOldestDataV(RP_CH_2, s2, buf2);

    // Resets trigger, but also defaults.
    rp_AcqReset();
}

int main(int argc, char **argv){
    // Parse arguments
    if (argc != 4) {
        exit(1);
    }
    float fstart = atof(argv[1]) * 1e3;
    float fend = atof(argv[2]) * 1e3;
    int nsteps = atoi(argv[3]);

    // Initialize IO.
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }
    // Initialize outputs but do not activate now
    rp_GenReset();
    rp_GenFreq(RP_CH_1, fstart);
    rp_GenFreq(RP_CH_2, fstart);
    rp_GenAmp(RP_CH_1, 0.5);
    rp_GenAmp(RP_CH_2, 0.5);
    rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
    rp_GenWaveform(RP_CH_2, RP_WAVEFORM_SINE);
    rp_GenMode(RP_CH_1, RP_GEN_MODE_CONTINUOUS);
    rp_GenMode(RP_CH_2, RP_GEN_MODE_CONTINUOUS);
    // Trigger setup only needed in burst mode.
    // Actually, setting triggers overwrites the mode.

    // Allocate data buffers
    float *buf1 = (float*)malloc(RP_BUFFER_SIZE * sizeof(float));
    float *buf2 = (float*)malloc(RP_BUFFER_SIZE * sizeof(float));

    // Scan
    for (int i = 0; i < nsteps; i++) {
        float f1 = fstart + i * (fend - fstart) / (nsteps-1);
        for (int j = 0; j < nsteps; j++) {
            float f2 = fstart + j * (fend - fstart) / (nsteps-1);
            fprintf(stderr, "%3.0f%% %.1fkHz %.1fkHz\n", 100.0*(nsteps*i+j)/(nsteps*nsteps-1), f1/1e3, f2/1e3);

            uint32_t s1 = RP_BUFFER_SIZE, s2 = RP_BUFFER_SIZE;
            acquire(f1, f2, buf1, &s1, buf2, &s2);

            printf("%f\t%f\t1", f1, f2);
            for (uint32_t k = 0; k < RP_BUFFER_SIZE; k++)
                printf("\t%f", buf1[k]);
            printf("\n%f\t%f\t2", f1, f2);
            for (uint32_t k = 0; k < RP_BUFFER_SIZE; k++)
                printf("\t%f", buf2[k]);
            printf("\n");
        }
    }

    free(buf1);
    free(buf2);
    rp_GenReset();
    rp_Release();
    return 0;
}
