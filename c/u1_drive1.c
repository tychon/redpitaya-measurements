/**
 * Drive output at one frequency given on command line, optionally
 * trigger GPIO initializer and record waveform at ADC.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include "redpitaya/rp.h"

#include "demodulation.h"
#include "utility.h"


// Additional settling time for output in microseconds
#define OUTPUT_SETTLING_TIME 10e3


int main(int argc, char **argv) {
    // Parse arguments
    if (argc < 2 || argc > 3) {
        exit(1);
    }
    float fout = atof(argv[1]);
    bool gpio_trig = argc == 3;

    // Initialize IO.
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    if (gpio_trig) { // Prepare trigger
      rp_DpinSetDirection(RP_DIO0_P, RP_IN);
      rp_DpinSetDirection(RP_DIO0_N, RP_OUT);
      rp_DpinSetState(RP_DIO0_N, RP_HIGH);
    }

    // Initialize outputs
    rp_GenReset();
    rp_GenFreq(RP_CH_1, fout);
    rp_GenAmp(RP_CH_1, 0.8);
    rp_GenOffset(RP_CH_1, 0);
    rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
    rp_GenMode(RP_CH_1, RP_GEN_MODE_CONTINUOUS);

    // Setup both ADC channels
    rp_AcqReset();
    rp_AcqSetGain(RP_CH_1, RP_LOW);
    rp_AcqSetGain(RP_CH_2, RP_LOW);
    rp_AcqSetDecimation(RP_DEC_8);
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_EXT_NE);
    rp_AcqSetTriggerDelay(7992); // max 8192
    rp_AcqSetAveraging(1);
    rp_AcqStart();

    rp_GenOutEnable(RP_CH_1);
    usleep(OUTPUT_SETTLING_TIME);

    // wait for "look ahead" buffer to fill up
    uint32_t buffertime = RP_BUFFER_SIZE * 8 / 125; // us
    usleep(buffertime);

    // trigger
    if (gpio_trig) rp_DpinSetState(RP_DIO0_N, RP_LOW);
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
