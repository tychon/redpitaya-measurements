/**
 * On trigger drive output CH1 at given frequency and phase.
 * Trigger signal generated on DIO0_N (negative edge).
 * DIO0_P (RP external trigger) and initializer of circuit
 * should be connected to this trigger.
 *
 * Phase variable is scanned over given region.
 *
 * Usage: ./run.sh IP u1_drive.x FREQ AMPLITUDE PHASESTART PHASEEND PHASESTEPS
 *
 * Data format (tab separated):
 *
 *     SAMPLERATE FREQ AMP PHASE CH SAMPLES...
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include "redpitaya/rp.h"

#include "utility.h"


int main(int argc, char **argv) {
    // Parse arguments
    if (argc != 6) {
        exit(1);
    }
    float freq = atof(argv[1]);
    float amp = atof(argv[2]);
    float phasestart = atof(argv[3]);
    float phaseend = atof(argv[4]);
    float phasensteps = atof(argv[5]);

    // Initialize IO.
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    rp_DpinSetDirection(RP_DIO0_P, RP_IN);
    rp_DpinSetDirection(RP_DIO0_N, RP_OUT);

    uint32_t bufsize = RP_BUFFER_SIZE;
    float *buf = (float *)malloc(RP_BUFFER_SIZE * sizeof(float));
    uint32_t buffertime = RP_BUFFER_SIZE * 8 / 125; // us

    for (int n = 0; n < phasensteps; n++) {
        float phase = phasestart + n * (phaseend-phasestart)/(phasensteps-1);
        fprintf(stderr, "phase =%6.1f\n", phase);

        // Prepare trigger
        rp_DpinSetState(RP_DIO0_N, RP_HIGH);

        // Initialize outputs
        rp_GenReset();
        rp_GenTriggerSource(RP_CH_1, RP_GEN_TRIG_SRC_EXT_NE);
        rp_GenFreq(RP_CH_1, freq);
        rp_GenAmp(RP_CH_1, amp);
        rp_GenOffset(RP_CH_1, 0);
        rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
        rp_GenPhase(RP_CH_1, phase);
        rp_GenMode(RP_CH_1, RP_GEN_MODE_BURST);
        rp_GenBurstCount(RP_CH_1, -1); // -1: continuous

        // Setup both ADC channels
        rp_AcqReset();
        rp_AcqSetGain(RP_CH_1, RP_HIGH);
        rp_AcqSetGain(RP_CH_2, RP_HIGH);
        rp_AcqSetDecimation(RP_DEC_1);
        rp_AcqSetTriggerSrc(RP_TRIG_SRC_EXT_NE);
        rp_AcqSetTriggerDelay(7992); // max 8192
        rp_AcqSetAveraging(1);
        rp_AcqStart();

        usleep(buffertime);

        // fire trigger
        rp_GenOutEnable(RP_CH_1);
        rp_DpinSetState(RP_DIO0_N, RP_LOW);
        // wait until acquisition trigger fired
        // (they do not have to be connected).
        rp_acq_trig_state_t state = RP_TRIG_STATE_WAITING;
        while (state != RP_TRIG_STATE_TRIGGERED) {
            rp_AcqGetTriggerState(&state);
        }
        usleep(buffertime);
        rp_GenOutDisable(RP_CH_1);

        // Retrieve data and print to stdout
        float samplerate;
        rp_AcqGetSamplingRateHz(&samplerate);

        rp_AcqGetOldestDataV(RP_CH_1, &bufsize, buf);
        printf("%f\t%f\t%f\t%f\t1", samplerate, freq, amp, phase);
        for(uint32_t i = 0; i < bufsize; i++) {
            printf("\t%f", buf[i]);
        }
        printf("\n");

        rp_AcqGetOldestDataV(RP_CH_2, &bufsize, buf);
        printf("%f\t%f\t%f\t%f\t2", samplerate, freq, amp, phase);
        for(uint32_t i = 0; i < bufsize; i++) {
            printf("\t%f", buf[i]);
        }
        printf("\n");

        rp_AcqReset();
    }

    free(buf);
    rp_GenReset();
    rp_Release();
    return 0;
}
