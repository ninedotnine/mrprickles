#ifndef MRPRICKLES_AV_CALLBACKS_H
#define MRPRICKLES_AV_CALLBACKS_H

#include <tox/toxav.h>

void call(ToxAV *toxAV, uint32_t friendNum, bool audio_enabled,
        bool video_enabled, __attribute__((unused)) void *user_data);

void call_state(ToxAV *toxAV, uint32_t friendNum, uint32_t state,
        __attribute__((unused)) void *user_data);

void audio_receive_frame(ToxAV *toxAV, uint32_t friendNum,
        const int16_t *pcm, size_t sample_count,
        uint8_t channels, uint32_t sampling_rate,
        __attribute__((unused)) void *user_data);

void video_receive_frame(ToxAV *toxAV, uint32_t friendNum, uint16_t width,
        uint16_t height, const uint8_t *y, const uint8_t *u,
        const uint8_t *v, int32_t ystride, int32_t ustride,
        int32_t vstride,
        __attribute__((unused)) void *user_data);


#endif
