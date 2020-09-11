/**
 * RP part for the live explorer. Streams ADC data to stdout to be
 * transmitted via SSH to python display script.
 *
 * Accepts driving frequency and amplitude commands for OUT1 on stdin
 * in the format:
 *
 *     FREQ AMP\n
 *
 * No triggering / initializer.
 * Output data format (tab separated) to stdout:
 *
 *     SAMPLERATE CH SAMPLES...
 *
 * Trigger position at sample 200
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include "redpitaya/rp.h"

#include "utility.h"


// Delay in us between buffer dumps
#define DUMP_SPEED 100000


int main(int argc, char **argv) {
    // Initialize IO.
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    uint32_t bufsize = ADC_BUFFER_SIZE;
    float *buf = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));

    /*
    // Initialize driving outputs
    rp_GenReset();
    // TODO start immediately
    rp_GenTriggerSource(RP_CH_1, RP_GEN_TRIG_SRC_EXT_NE);
    rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
    rp_GenFreq(RP_CH_1, f);
    rp_GenAmp(RP_CH_1, amp);
    rp_GenPhase(RP_CH_1, phase);
    rp_GenOffset(RP_CH_1, 0);
    rp_GenMode(RP_CH_1, RP_GEN_MODE_BURST);
    rp_GenBurstCount(RP_CH_1, -1); // -1: continuous
    float outfreq = 0;
    float outamp = 0;
    */

    // Setup both ADC channels
    rp_AcqReset();
    rp_AcqSetGain(RP_CH_1, RP_HIGH);
    rp_AcqSetGain(RP_CH_2, RP_HIGH);
    rp_AcqSetDecimation(RP_DEC_64);
    rp_AcqSetArmKeep(true); // keep going after trigger
    rp_AcqSetAveraging(1);
    rp_AcqStart();
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);

    float samplerate;
    rp_AcqGetSamplingRateHz(&samplerate);
    uint32_t buffertime = (uint32_t)((float)ADC_BUFFER_SIZE / samplerate * 1e6); // us

    while (true) {
        usleep(buffertime);
        if (DUMP_SPEED > buffertime)
            usleep(DUMP_SPEED - buffertime);

        rp_AcqGetOldestDataV(RP_CH_1, &bufsize, buf);
        printf("%f\t1", samplerate);
        for(uint32_t i = 0; i < bufsize; i++) {
            printf("\t%.3f", buf[i]);
        }
        printf("\n");

        rp_AcqGetOldestDataV(RP_CH_2, &bufsize, buf);
        printf("%f\t2", samplerate);
        for(uint32_t i = 0; i < bufsize; i++) {
            printf("\t%.3f", buf[i]);
        }
        printf("\n");
        fflush(stdout);
    }

    free(buf);
    rp_GenReset();
    rp_Release();
    return 0;
}
