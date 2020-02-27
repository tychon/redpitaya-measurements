
#include <unistd.h>
#include <math.h>

#include "utility.h"


float log_scale_steps(int i, int imax, float vmin, float vmax) {
    return exp(log(vmin) + i * log(vmax/vmin) / (imax-1));
}


rp_acq_decimation_t best_decimation_factor(float f, float *samplerate) {
    // Ideal decimation factor for oversampling of 20
    float targetdec = RP_BASE_SAMPLERATE / (f * 20);

    // Choose the largest allowed decimation factor that is smaller
    // than `targetdec`.
    if (targetdec >= 65536) {
        *samplerate = RP_BASE_SAMPLERATE / 65536;
        return RP_DEC_65536;
    } else if (targetdec >= 8192) {
        *samplerate = RP_BASE_SAMPLERATE / 8192;
        return RP_DEC_8192;
    } else if (targetdec >= 1024) {
        *samplerate = RP_BASE_SAMPLERATE / 1024;
        return RP_DEC_1024;
    } else if (targetdec >= 64) {
        *samplerate = RP_BASE_SAMPLERATE / 64;
        return RP_DEC_64;
    } else if (targetdec > 8) {
        *samplerate = RP_BASE_SAMPLERATE / 8;
        return RP_DEC_8;
    } else {
        *samplerate = RP_BASE_SAMPLERATE;
        return RP_DEC_1;
    }
}


void acquire_2channels(
        const rp_acq_decimation_t decimation,
        float *buf1, uint32_t *s1,
        float *buf2, uint32_t *s2) {
    // Resets trigger, but also defaults.
    rp_AcqReset();

    rp_AcqSetGain(RP_CH_1, RP_LOW);
    rp_AcqSetGain(RP_CH_2, RP_LOW);
    rp_AcqSetDecimation(decimation);
    rp_AcqSetTriggerDelay(8192);
    rp_AcqSetAveraging(1);
    rp_AcqStart();

    float samplerate;
    rp_AcqGetSamplingRateHz(&samplerate);

    // time frame of one buffer in us
    uint32_t buffertime = 1e6 * RP_BUFFER_SIZE / samplerate;
    // wait for "look ahead" buffer to fill up
    usleep(buffertime);

    // wait for trigger
    rp_AcqSetTriggerSrc(RP_TRIG_SRC_NOW);
    rp_acq_trig_state_t state = RP_TRIG_STATE_TRIGGERED;
    while (1) {
        rp_AcqGetTriggerState(&state);
        if(state == RP_TRIG_STATE_TRIGGERED){
            break;
        }
    }
    usleep(buffertime);

    // Retrieve data
    rp_AcqGetOldestDataV(RP_CH_1, s1, buf1);
    rp_AcqGetOldestDataV(RP_CH_2, s2, buf2);
}
