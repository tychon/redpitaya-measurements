/**
 * RP part for the live explorer. Streams ADC data to stdout to be
 * transmitted via SSH to python display script.
 *
 * Accepts driving frequency and amplitude commands for OUT1 on stdin
 * in the format:
 *
 *     FREQ AMP\n
 *
 * Output data format (tab separated) to stdout:
 *
 *     CH SAMPLES...
 *
 * Trigger position at sample 200
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include "rp.h"

#include "utility.h"


// Delay in us between triggers / buffer dumps
#define CHAIN_LEADER_DELAY_US 300000

int main(int argc, char **argv) {
    // Initialize IO.
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    // Prepare trigger
    // DIO0_P is trigger input line / EXT_TRIg
    rp_DpinSetDirection(RP_DIO0_P, RP_IN);
    // DIO0_N is trigger output line
    rp_DpinSetDirection(RP_DIO0_N, RP_OUT);
    // DIO1_P is GND reference for initializer pre-stage
    // and always set to LOW.
    rp_DpinSetDirection(RP_DIO1_P, RP_OUT);
    rp_DpinSetState(RP_DIO0_N, RP_LOW);
    rp_DpinSetState(RP_DIO1_P, RP_LOW);

    uint32_t buffertime = ADC_BUFFER_SIZE * 64 / 125; // us

    uint32_t bufsize = ADC_BUFFER_SIZE;
    float *buf = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));

    long int idx = 0;
    while (true) {
        //usleep(buffertime);
        //if (DUMP_SPEED > buffertime)
        //    usleep(DUMP_SPEED - buffertime);

        rp_DpinSetState(RP_DIO0_N, RP_HIGH);

        // Setup both ADC channels
        rp_AcqReset();
        rp_AcqSetGain(RP_CH_1, RP_HIGH);
        rp_AcqSetGain(RP_CH_2, RP_HIGH);
        rp_AcqSetDecimation(RP_DEC_64);
        rp_AcqSetTriggerDelay(7992); // trigger at sample 200
        rp_AcqSetAveraging(1);
        rp_AcqStart();

        rp_AcqSetTriggerSrc(RP_TRIG_SRC_EXT_NE);
#ifndef FOLLOW
        // Leader of a chain of Red Pitayas waits
        // so that others can catch up to here and are actually
        // also awaiting the next trigger signal.
        usleep(CHAIN_LEADER_DELAY_US);
#endif
        rp_DpinSetState(RP_DIO0_N, RP_LOW);

        // Wait until acquisition trigger fired
        rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
        while (1) {
            rp_AcqGetTriggerState(&state);
            if(state == RP_TRIG_STATE_TRIGGERED){
                break;
            }
        }
        // Wait until ADC buffer is full
        usleep(buffertime);

        rp_AcqGetOldestDataV(RP_CH_2, &bufsize, buf);
        printf("%ld\t2", idx);
        for(uint32_t i = 0; i < bufsize; i++) {
            printf("\t%.3f", buf[i]);
        }
        printf("\n");

        rp_AcqGetOldestDataV(RP_CH_1, &bufsize, buf);
        printf("%ld\t1", idx);
        for(uint32_t i = 0; i < bufsize; i++) {
            printf("\t%.3f", buf[i]);
        }
        printf("\n");

        fflush(stdout);
        idx ++;
    }

    free(buf);
    rp_GenReset();
    rp_Release();
    return 0;
}
