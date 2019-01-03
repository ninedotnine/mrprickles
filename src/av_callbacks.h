#pragma once

#include "globals.h"

#include <tox/toxav.h>

void call(ToxAV *toxAV, uint32_t friend_num, bool audio_enabled, bool video_enabled, GCC_UNUSED void *user_data);

void call_state(ToxAV *toxAV, uint32_t friend_num, uint32_t state, GCC_UNUSED void *user_data);

void audio_receive_frame(ToxAV *toxAV, uint32_t friend_num,
                        const int16_t *pcm, size_t sample_count,
                        uint8_t channels, uint32_t sampling_rate,
                        GCC_UNUSED void *user_data);

void video_receive_frame(ToxAV *toxAV, uint32_t friend_num, uint16_t width, uint16_t height,
                        const uint8_t *y, const uint8_t *u, const uint8_t *v,
                        int32_t ystride, int32_t ustride, int32_t vstride,
                        GCC_UNUSED void *user_data);
