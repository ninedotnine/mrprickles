#include "av_callbacks.h"

#include "globals.h"
#include "util.h"

#include <string.h>
#include <stdlib.h>

void call(ToxAV *toxAV, uint32_t friend_num, bool audio_enabled, bool video_enabled, GCC_UNUSED void *user_data) {
    uint8_t * friend_name;
    friend_name_from_num(&friend_name, toxav_get_tox(toxAV), friend_num);
    TOXAV_ERR_ANSWER err;
    toxav_answer(toxAV, friend_num, audio_enabled ? audio_bitrate : 0,
            video_enabled ? video_bitrate : 0, &err);

    if (err == TOXAV_ERR_ANSWER_OK) {
        logger("answered call from friend %u (%s).", friend_num, friend_name);
    } else {
        logger("could not answer call, friend: %u (%s), error: %d",
                friend_num, friend_name, err);
    }
    free(friend_name);
}

void call_state(ToxAV *toxAV, uint32_t friend_num, uint32_t state, GCC_UNUSED void *user_data) {
    uint8_t * friend_name;
    friend_name_from_num(&friend_name, toxav_get_tox(toxAV), friend_num);
    if (state & TOXAV_FRIEND_CALL_STATE_FINISHED) {
        logger("call with friend %u (%s) finished", friend_num, friend_name);
        return;
    } else if (state & TOXAV_FRIEND_CALL_STATE_ERROR) {
        logger("call with friend %u (%s) errored", friend_num, friend_name);
        return;
    }

    bool send_audio = (state & TOXAV_FRIEND_CALL_STATE_SENDING_A)
        && (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A);
    bool send_video = state & TOXAV_FRIEND_CALL_STATE_SENDING_V
        && (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_V);

    TOXAV_ERR_BIT_RATE_SET audio_err;
    TOXAV_ERR_BIT_RATE_SET video_err;
    toxav_audio_set_bit_rate(toxAV, friend_num, send_audio ? audio_bitrate : 0,
                             &audio_err);
    if (audio_err != TOXAV_ERR_BIT_RATE_SET_OK) {
        logger("audio bit rate failed to set.");
    }
    toxav_video_set_bit_rate(toxAV, friend_num, send_audio ? video_bitrate : 0,
                             &video_err);
    if (audio_err != TOXAV_ERR_BIT_RATE_SET_OK) {
        logger("video bit rate failed to set.");
    }

    logger("call state for friend %u (%s) changed to %u: audio: %d, video: %d",
            friend_num, friend_name, state, send_audio, send_video);
    free(friend_name);
}

void audio_receive_frame(ToxAV *toxAV, uint32_t friend_num, const int16_t *pcm, size_t sample_count,
                        uint8_t channels, uint32_t sampling_rate, GCC_UNUSED void *user_data) {
    TOXAV_ERR_SEND_FRAME err;
    toxav_audio_send_frame(toxAV, friend_num, pcm, sample_count, channels,
            sampling_rate, &err);

    if (err != TOXAV_ERR_SEND_FRAME_OK) {
        logger("could not send audio frame to friend: %u, error: %d",
                friend_num, err);
    }
}

void video_receive_frame(ToxAV *toxAV, uint32_t friend_num, uint16_t width, uint16_t height,
                        const uint8_t *y, const uint8_t *u, const uint8_t *v,
                        const int32_t ystride, const int32_t ustride, const int32_t vstride,
                        GCC_UNUSED void *user_data) {
    uint32_t ystride_abs = (uint32_t) abs(ystride);
    uint32_t ustride_abs = (uint32_t) abs(ustride);
    uint32_t vstride_abs = (uint32_t) abs(vstride);

    if (ystride_abs < width || ustride_abs < width / 2 || vstride_abs < width / 2) {
        logger("wtf");
        return;
    }

    if (height == 0) {
        logger("height of frame should not be zero.");
        return;
    }
    uint8_t *y_dest = calloc(width, height);
    uint8_t *u_dest = calloc(width, height / 2);
    uint8_t *v_dest = calloc(width, height / 2);

    for (size_t h = 0; h < height; h++) {
        memcpy(&y_dest[h * width], &y[h * ystride_abs], width);
    }

    for (size_t h = 0; h < height / 2; h++) {
        memcpy(&u_dest[h * width / 2], &u[h * ustride_abs], width / 2);
        memcpy(&v_dest[h * width / 2], &v[h * vstride_abs], width / 2);
    }

    TOXAV_ERR_SEND_FRAME err;
    toxav_video_send_frame(toxAV, friend_num, width, height, y_dest,
            u_dest, v_dest, &err);

    free(y_dest);
    free(u_dest);
    free(v_dest);

    if (err != TOXAV_ERR_SEND_FRAME_OK) {
        logger("could not send video frame to friend: %u, error: %d",
                friend_num, err);
    }
}
