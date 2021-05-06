/**
 * Drive outputs at given frequencies and and phase and record ADC.
 * Output and recording triggered by  negative edge of external trigger DIO0_P
 * of extension connector E1 (3.3V, negative edge).
 *
 * Provides trigger on digital pin DIO0_N of
 * extension connector E1 (3.3V, negative edge).
 * Connect DIO0_N to DIO0_P to synchronize.
 *
 * Usage: u1_drive2 FREQ1 AMP1 PHASE1 FREQ2 AMP2 PHASE2
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include "rp.h"

#include "demodulation.h"
#include "utility.h"


int main(int argc, char **argv) {
    // Parse arguments
    if (argc != 7) {
        fprintf(stderr, "Invalid arguments.\n");
        exit(1);
    }
    float freq1 = atof(argv[1]);
    float amp1 = atof(argv[2]);
    float phase1 = atof(argv[3]);
    float freq2 = atof(argv[4]);
    float amp2 = atof(argv[5]);
    float phase2 = atof(argv[6]);

    // Initialize IO.
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    // Prepare GPIO trigger
    rp_DpinSetDirection(RP_DIO0_P, RP_IN);
    rp_DpinSetDirection(RP_DIO0_N, RP_OUT);
    rp_DpinSetState(RP_DIO0_N, RP_HIGH);

    // Initialize outputs
    rp_GenReset();
    rp_GenTriggerSource(RP_CH_1, RP_GEN_TRIG_SRC_EXT_NE);
    rp_GenTriggerSource(RP_CH_2, RP_GEN_TRIG_SRC_EXT_NE);
    rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
    rp_GenWaveform(RP_CH_2, RP_WAVEFORM_SINE);
    rp_GenFreq(RP_CH_1, freq1);
    rp_GenFreq(RP_CH_2, freq2);
    rp_GenAmp(RP_CH_1, amp1);
    rp_GenAmp(RP_CH_2, amp2);
    rp_GenPhase(RP_CH_1, phase1);
    rp_GenPhase(RP_CH_2, phase2);
    rp_GenOffset(RP_CH_1, 0);
    rp_GenOffset(RP_CH_2, 0);
    rp_GenMode(RP_CH_1, RP_GEN_MODE_BURST);
    rp_GenMode(RP_CH_2, RP_GEN_MODE_BURST);
    rp_GenBurstCount(RP_CH_1, -1); // -1: continuous
    rp_GenBurstCount(RP_CH_2, -1); // -1: continuous

    // Setup both ADC channels
    rp_AcqReset();
    rp_AcqSetGain(RP_CH_1, RP_HIGH);
    rp_AcqSetGain(RP_CH_2, RP_HIGH);
    rp_AcqSetDecimation(RP_DEC_8);
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_EXT_NE);
    rp_AcqSetTriggerDelay(7992); // trigger at sample 200
    rp_AcqSetAveraging(1);
    rp_AcqStart();

    rp_GenOutEnable(RP_CH_1);
    rp_GenOutEnable(RP_CH_2);

    // Wait for "look ahead" buffer to fill up
    uint32_t buffertime = ADC_BUFFER_SIZE * 8 / 125; // us
    usleep(buffertime);

    // Fire trigger
    rp_DpinSetState(RP_DIO0_N, RP_LOW);
    // Wait until acquisition trigger fired
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
    uint32_t bufsize1 = ADC_BUFFER_SIZE;
    float *buf1 = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));
    rp_AcqGetOldestDataV(RP_CH_1, &bufsize1, buf1);

    uint32_t bufsize2 = ADC_BUFFER_SIZE;
    float *buf2 = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));
    rp_AcqGetOldestDataV(RP_CH_2, &bufsize2, buf2);

    rp_AcqReset();

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
