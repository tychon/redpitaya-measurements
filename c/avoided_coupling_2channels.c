/**
 * Drive two oscillators via RF outputs and scan frequency space.
 * Measure output voltage via RF inputs to calculate external load.
 * This code also runs demodulation and outputs only the metadata.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "redpitaya/rp.h"


int main(int argc, char **argv){
    /* Print error, if rp_Init() function failed */
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    // Setup both generator channels
    rp_GenReset();
    rp_GenFreq(RP_CH_1, 120000);
    rp_GenFreq(RP_CH_2, 120000);
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

    rp_AcqStart(); // start acquiring
    rp_GenOutEnable(RP_CH_1);
    rp_GenOutEnable(RP_CH_2);

    // time frame of one buffer in us
    const uint32_t maxbufsize = 16384;
    uint32_t buffertime = maxbufsize * 8 / 125;
    // wait for "look ahead" buffer to fill up
    usleep(buffertime);

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

    uint32_t bufsize = maxbufsize;
    float *buf = (float *)malloc(maxbufsize * sizeof(float));
    rp_AcqGetOldestDataV(RP_CH_1, &bufsize, buf);

    rp_GenReset();
    rp_AcqReset();

    int i;
    for(i = 0; i < bufsize; i++){
        printf("%f\n", buf[i]);
    }

    free(buf);
    rp_Release();

    return 0;
}
