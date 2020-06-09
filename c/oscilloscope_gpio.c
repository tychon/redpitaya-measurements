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
 * Prints to stdout the ADC buffers, trigger position at sample 200.
 * Formatted tab-separated two columns, i.e. one for each channel.
 *
 * Note: Default setting of digital IO pins is OUT, LOW.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "redpitaya/rp.h"

#include "utility.h"


#define RP_GEN_SAMPLERATE 125e6


int main(int argc, char **argv){
    float ttlCH2_delay = -1;
    if (argc == 2) {
        ttlCH2_delay = atof(argv[1]);
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
    rp_DpinSetState(RP_DIO0_N, RP_HIGH);

    // Setup both ADC channels
    rp_AcqReset();
    rp_AcqSetGain(RP_CH_1, RP_HIGH);
    rp_AcqSetGain(RP_CH_2, RP_HIGH);
    rp_AcqSetDecimation(RP_DEC_8);
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_EXT_NE);
    rp_AcqSetTriggerDelay(7992); // trigger at sample 200
    rp_AcqSetAveraging(1);
    rp_AcqStart();

    // Prepare CH2 trigger using arbitrary waveform
    if (ttlCH2_delay >= 0) {
        float *trigwaveform = (float *)malloc(ADC_BUFFER_SIZE * sizeof(float));
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
        free(trigwaveform);
    }

    // Wait for "look ahead" buffer to fill up
    uint32_t buffertime = ADC_BUFFER_SIZE * 8 / 125; // us
    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
    usleep(buffertime);

    // Fire trigger
    rp_DpinSetState(RP_DIO0_N, RP_LOW);

    // Wait until acquisition trigger fired
    while (1) {
        rp_AcqGetTriggerState(&state);
        if(state == RP_TRIG_STATE_TRIGGERED){
            break;
        }
    }
    if (ttlCH2_delay >= 0) {
        // Wait for delayed CH2 trigger
        usleep(ttlCH2_delay * 1e6);
        // Prevent CH2 trigger from returning to high
        rp_GenOutDisable(RP_CH_2);
    }
    // Wait until ADC buffer is full
    usleep(buffertime);

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
    rp_Release();
    return 0;
}
