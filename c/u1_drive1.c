/**
 * Drive output CH1 at given frequency and and phase and record ADC.
 * Output and recording triggered by  negative edge of external trigger DIO0_P
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
 * Usage: u1_drive1 FREQ AMPLITUDE PHASE CH2DELAY [CHNUMOFFSET]
 *
 * You may give ranges for any of the arguments by using
 * START,NPOINTS,END for e.g. FREQ.  CHNUMOFFSET is added to the
 * channel numbers to allow combining output from multiple Red
 * Pitayas.
 *
 * Output data format (tab separated) to stdout:
 *
 *     SAMPLERATE FREQ AMP PHASE CH2DELAY CH SAMPLES...
 *
 * Trigger position at sample 200
 *
 * Note: Default setting of digital IO pins is OUT, LOW.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <math.h>

#include "rp.h"

#include "demodulation.h"
#include "utility.h"


#define RP_GEN_SAMPLERATE 125e6
#define CHAIN_LEADER_DELAY_US 100000


int main(int argc, char **argv) {
    // Parse arguments
    float f_start, f_end, amp_start, amp_end, phase_start, phase_end,
        ttlCH2_start = 0, ttlCH2_end = 0;
    int f_npoints, amp_npoints, phase_npoints, ttlCH2_npoints = 1;
    int chnumoffset = 0;
    if (argc >= 5) {
        if (!parse_cmd_line_range(argv[1], &f_start, &f_end, &f_npoints)
            || !parse_cmd_line_range(argv[2], &amp_start, &amp_end, &amp_npoints)
            || !parse_cmd_line_range(argv[3], &phase_start, &phase_end, &phase_npoints)
            || !parse_cmd_line_range(argv[4], &ttlCH2_start, &ttlCH2_end, &ttlCH2_npoints)) {
            fprintf(stderr, "Invalid argument.\n");
            exit(1);
        }
    }
    if (argc == 6) {
        chnumoffset = strtol(argv[5], NULL, 10);
    }
    if (argc < 5 || argc > 6) {
        fprintf(stderr, "Invalid number of arguments.\n");
        exit(1);
    }

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

    float *trigwaveform = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));

    uint32_t bufsize = ADC_BUFFER_SIZE;
    float *buf = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));

    /* // print header
    printf("samplerate\tf\tamplitude\tphase\tch2delay\tch");
    for(uint32_t i = 0; i < bufsize; i++) {
        printf("\t%d", i);
    }
    printf("\n"); */

    int itotal = 0;
    for (int f_i = 0; f_i < f_npoints; f_i++) {
        float f = lin_scale_steps(f_i, f_npoints, f_start, f_end);
        for (int amp_i = 0; amp_i < amp_npoints; amp_i++) {
            float amp = lin_scale_steps(amp_i, amp_npoints, amp_start, amp_end);
            for (int phase_i = 0; phase_i < phase_npoints; phase_i++) {
                float phase = lin_scale_steps(phase_i, phase_npoints, phase_start, phase_end);
                for (int ttlCH2_i = 0; ttlCH2_i < ttlCH2_npoints; ttlCH2_i++) {
                    float ttlCH2_delay = lin_scale_steps(
                        ttlCH2_i, ttlCH2_npoints, ttlCH2_start, ttlCH2_end);

                    fprintf(stderr, "%3.0f%% %.2fkHz %.3fV %.1f° %.2fus\n",
                            100.0*itotal/(f_npoints*amp_npoints*phase_npoints*ttlCH2_npoints-1),
                            f/1e3, amp, phase, ttlCH2_delay*1e6);

                    rp_DpinSetState(RP_DIO0_N, RP_HIGH);

                    // Initialize driving outputs
                    rp_GenReset();
                    rp_GenTriggerSource(RP_CH_1, RP_GEN_TRIG_SRC_EXT_NE);
                    rp_GenWaveform(RP_CH_1, RP_WAVEFORM_SINE);
                    rp_GenFreq(RP_CH_1, f);
                    rp_GenAmp(RP_CH_1, amp);
                    rp_GenPhase(RP_CH_1, phase);
                    rp_GenOffset(RP_CH_1, 0);
                    rp_GenMode(RP_CH_1, RP_GEN_MODE_BURST);
                    rp_GenBurstCount(RP_CH_1, -1); // -1: continuous

                    // Initialize TTL line output
                    ttl_arb_waveform(RP_GEN_SAMPLERATE, ttlCH2_delay, trigwaveform, ADC_BUFFER_SIZE);
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

                    // Setup both ADC channels
                    rp_AcqReset();
                    rp_AcqSetGain(RP_CH_1, RP_HIGH);
                    rp_AcqSetGain(RP_CH_2, RP_HIGH);
                    rp_AcqSetDecimation(RP_DEC_64);
                    rp_AcqSetTriggerSrc(RP_TRIG_SRC_EXT_NE);
                    rp_AcqSetTriggerDelay(7992); // trigger at sample 200
                    rp_AcqSetAveraging(1);
                    rp_AcqStart();

                    // Wait for "look ahead" buffer to fill up
                    uint32_t buffertime = ADC_BUFFER_SIZE * 64 / 125; // us
                    usleep(buffertime);

                    // Fire trigger
                    rp_GenOutEnable(RP_CH_1);
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
                    // Wait for delayed CH2 trigger
                    usleep(ttlCH2_delay * 1e6);
                    // Prevent CH2 trigger from returning to high
                    rp_GenOutDisable(RP_CH_2);

                    // Wait until ADC buffer is full
                    usleep(buffertime);
                    rp_GenOutDisable(RP_CH_1);

                    // Retrieve data and print data to stdout
                    float samplerate;
                    rp_AcqGetSamplingRateHz(&samplerate);

                    rp_AcqGetOldestDataV(RP_CH_1, &bufsize, buf);
                    printf("%f\t%f\t%f\t%f\t%f\t%d", samplerate, f, amp, phase,
                           ttlCH2_delay, 1+chnumoffset);
                    for(uint32_t i = 0; i < bufsize; i++) {
                        printf("\t%f", buf[i]);
                    }
                    printf("\n");
                    
                    rp_AcqGetOldestDataV(RP_CH_2, &bufsize, buf);
                    printf("%f\t%f\t%f\t%f\t%f\t%d", samplerate, f, amp, phase,
                           ttlCH2_delay, 2+chnumoffset);
                    for(uint32_t i = 0; i < bufsize; i++) {
                        printf("\t%f", buf[i]);
                    }
                    printf("\n");

                    itotal ++;
                }
            }
        }
    }

    free(trigwaveform);
    free(buf);
    rp_GenReset();
    rp_Release();
    return 0;
}
