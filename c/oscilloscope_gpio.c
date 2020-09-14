/**
 * Acquire signal on RF analog inputs @ 125Msps/8.
 * Signal is recorded at negative edge of external trigger DIO0_P
 * of extension connector E1 (3.3V, negative edge).
 *
 * Generates trigger on digital pin DIO0_N of
 * extension connector E1 (3.3V, negative edge).
 * Connect DIO0_N to DIO0_P to synchronize.
 *
 * Optionally generates trigger on fast analog output CH2
 * (1V, negative edge) at a delay (in seconds) specified as
 * cmd line argument.  This trigger has an additional latency
 * of 0.2 to 0.3 microseconds.
 *
 * Usage: oscilloscope_gpio [CH2DELAY]
 *
 * You may give a range for for CH2DELAY by using
 * START,NPOINTS,END
 *
 * Output data format (tab separated) to stdout:
 *
 *     SAMPLERATE CH2DELAY CH SAMPLES...
 *
 * Trigger position at sample 200
 *
 * Note: Default setting of digital IO pins is OUT, LOW.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "redpitaya/rp.h"

#include "utility.h"


#define RP_GEN_SAMPLERATE 125e6
#define CHAIN_LEADER_DELAY_US 100000


int main(int argc, char **argv){
    float ttlCH2_start = 0, ttlCH2_end = 0;
    int ttlCH2_npoints = 1;
    bool ttlCH2 = false;
    if (argc == 2) {
        ttlCH2 = true;
        if (!parse_cmd_line_range(argv[1], &ttlCH2_start, &ttlCH2_end, &ttlCH2_npoints)) {
            fprintf(stderr, "Invalid argument.\n");
            exit(1);
        }
    } else if (argc != 1) {
        fprintf(stderr, "Invalid arguments.\n");
        exit(1);
    }

    // Print error, if rp_Init() function failed
    if (rp_Init() != RP_OK) {
        fprintf(stderr, "RP api init failed!\n");
        exit(2);
    }

    // Prepare GPIO trigger
    rp_DpinSetDirection(RP_DIO0_P, RP_IN);
    rp_DpinSetDirection(RP_DIO0_N, RP_OUT);
    // DIO1_P is GND reference for initializer pre-stage
    // and always set to LOW.
    rp_DpinSetDirection(RP_DIO1_P, RP_OUT);
    rp_DpinSetState(RP_DIO0_N, RP_LOW);
    rp_DpinSetState(RP_DIO1_P, RP_LOW);

    uint32_t bufsize = ADC_BUFFER_SIZE;
    float *buf = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));
    float *trigwaveform = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));

    for (int ttlCH2_i = 0; ttlCH2_i < ttlCH2_npoints; ttlCH2_i++) {
        float ttlCH2_delay = lin_scale_steps(
            ttlCH2_i, ttlCH2_npoints, ttlCH2_start, ttlCH2_end);
        if (ttlCH2) {
            fprintf(stderr, "%3.0f%% %.2fus\n",
                    100.0*ttlCH2_i/(ttlCH2_npoints-1), ttlCH2_delay*1e6);
        }

        rp_DpinSetState(RP_DIO0_N, RP_HIGH);

        // Setup both ADC channels
        rp_AcqReset();
        rp_AcqSetGain(RP_CH_1, RP_HIGH);
        rp_AcqSetGain(RP_CH_2, RP_HIGH);
        rp_AcqSetDecimation(RP_DEC_64);
        rp_AcqSetTriggerSrc(RP_TRIG_SRC_EXT_NE);
        rp_AcqSetTriggerDelay(7992); // trigger at sample 200
        rp_AcqSetAveraging(1);
        rp_AcqStart();

        // Prepare CH2 trigger using arbitrary waveform
        if (ttlCH2) {
            rp_GenReset();
            ttl_arb_waveform(RP_GEN_SAMPLERATE, ttlCH2_delay, trigwaveform, ADC_BUFFER_SIZE);
            rp_GenReset();
            rp_GenTriggerSource(RP_CH_2, RP_GEN_TRIG_SRC_EXT_NE);
            rp_GenWaveform(RP_CH_2, RP_WAVEFORM_ARBITRARY);
            rp_GenArbWaveform(RP_CH_2, trigwaveform, ADC_BUFFER_SIZE);
            rp_GenFreq(RP_CH_2, RP_GEN_SAMPLERATE/ADC_BUFFER_SIZE);
            rp_GenAmp(RP_CH_2, 1);
            rp_GenMode(RP_CH_2, RP_GEN_MODE_BURST);
            rp_GenBurstCount(RP_CH_2, 1);
            // Enable output (first sample always high) before `usleep`
            // to let ringing dissipate.
            rp_GenOutEnable(RP_CH_2);
        }

        // Wait for "look ahead" buffer to fill up
        float samplerate;
        rp_AcqGetSamplingRateHz(&samplerate);
        uint32_t buffertime = (uint32_t)(1e6 * ADC_BUFFER_SIZE / samplerate); // us
        rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
        usleep(buffertime);

        // Fire trigger
#ifndef FOLLOW
        // Leader of a chain of Red Pitayas waits
        // so that others can catch up to here and are actually
        // also awaiting the next trigger signal.
        usleep(CHAIN_LEADER_DELAY_US);
#endif
        rp_DpinSetState(RP_DIO0_N, RP_LOW);

        // Wait until acquisition trigger fired
        while (1) {
            rp_AcqGetTriggerState(&state);
            if(state == RP_TRIG_STATE_TRIGGERED){
                break;
            }
        }
        if (ttlCH2 >= 0) {
            // Wait for delayed CH2 trigger
            usleep(ttlCH2_delay * 1e6);
            // Prevent CH2 trigger from returning to high
            rp_GenOutDisable(RP_CH_2);
        }
        // Wait until ADC buffer is full
        usleep(buffertime);

        // Retrieve data and print data to stdout
        rp_AcqGetOldestDataV(RP_CH_1, &bufsize, buf);
        printf("%f\t%f\t1", samplerate, ttlCH2_delay);
        for(uint32_t i = 0; i < bufsize; i++) {
            printf("\t%f", buf[i]);
        }
        printf("\n");

        rp_AcqGetOldestDataV(RP_CH_2, &bufsize, buf);
        printf("%f\t%f\t2", samplerate, ttlCH2_delay);
        for(uint32_t i = 0; i < bufsize; i++) {
            printf("\t%f", buf[i]);
        }
        printf("\n");
    }

    free(trigwaveform);
    free(buf);
    rp_GenReset();
    rp_Release();
    return 0;
}
