/**
 * On trigger drive output CH1 at given frequency and phase.
 * Trigger signal generated on DIO0_N (negative edge).
 * DIO0_P (RP external trigger) and initializer of circuit
 * should be connected to this trigger.
 *
 * Usage: ./run.sh IP u1_drive.x FREQ AMPLITUDE PHASE
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include "redpitaya/rp.h"

#include "demodulation.h"
#include "utility.h"


int main(int argc, char **argv) {
    // Parse arguments
    if (argc != 4) {
        exit(1);
    }
    float fout = atof(argv[1]);
    float amp = atof(argv[2]);
    float phase = atof(argv[3]);

    // Initialize IO.
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    // Prepare trigger
    rp_DpinSetDirection(RP_DIO0_P, RP_IN);
    rp_DpinSetDirection(RP_DIO0_N, RP_OUT);
    rp_DpinSetState(RP_DIO0_N, RP_HIGH);

    // Initialize outputs
    rp_GenReset();
    rp_GenTriggerSource(RP_CH_1, RP_GEN_TRIG_SRC_EXT_NE);
    rp_GenFreq(RP_CH_1, fout);
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

    // wait for "look ahead" buffer to fill up
    uint32_t buffertime = RP_BUFFER_SIZE * 8 / 125; // us
    usleep(buffertime);

    // fire trigger
    rp_GenOutEnable(RP_CH_1);
    rp_DpinSetState(RP_DIO0_N, RP_LOW);
    // wait until acquisition trigger fired
    // (they do not have to be connected).
    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
    while (1) {
        rp_AcqGetTriggerState(&state);
        if(state == RP_TRIG_STATE_TRIGGERED){
            break;
        }
    }
    usleep(buffertime);

    // Retrieve data
    uint32_t bufsize1 = RP_BUFFER_SIZE;
    float *buf1 = (float *)malloc(RP_BUFFER_SIZE * sizeof(float));
    rp_AcqGetOldestDataV(RP_CH_1, &bufsize1, buf1);

    uint32_t bufsize2 = RP_BUFFER_SIZE;
    float *buf2 = (float *)malloc(RP_BUFFER_SIZE * sizeof(float));
    rp_AcqGetOldestDataV(RP_CH_2, &bufsize2, buf2);

    rp_AcqReset();
    rp_GenOutDisable(RP_CH_1);

    // Print data to stdout
    for(uint32_t i = 0; i < bufsize1 && i < bufsize2; i++){
        printf("%f\t%f\n", buf1[i], buf2[i]);
    }

    free(buf1);
    free(buf2);
    rp_GenReset();
    rp_Release();
    return 0;
}
