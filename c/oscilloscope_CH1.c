/**
 * Acquire signal on RF analog inputs @ 125Msps/8.
 * Take data after trigger (rising edge CH1 > 0.5V).
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "rp.h"

// Number of samples in ADC buffer, equal to 2**14.
#define RP_BUFFER_SIZE 16384


int main(int argc, char **argv){
    // Print error, if rp_Init() function failed
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    // Setup both ADC channels
    rp_AcqReset();
    rp_AcqSetGain(RP_CH_1, RP_HIGH);
    rp_AcqSetGain(RP_CH_2, RP_HIGH);
    rp_AcqSetDecimation(RP_DEC_8);
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_CHA_PE);
    rp_AcqSetTriggerLevel(RP_CH_1, 0.1);
    rp_AcqSetTriggerDelay(7992); // trigger at sample 200
    rp_AcqSetAveraging(1);
    rp_AcqStart();

    // wait for "look ahead" buffer to fill up
    uint32_t buffertime = RP_BUFFER_SIZE * 8 / 125; // us
    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
    usleep(buffertime);
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

    // Print data to stdout
    for(uint32_t i = 0; i < bufsize1 && i < bufsize2; i++){
        printf("%f\t%f\n", buf1[i], buf2[i]);
    }

    free(buf1);
    free(buf2);
    rp_Release();
    return 0;
}
