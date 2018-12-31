#ifndef MRPRICKLES_CALLBACKS_H
#define MRPRICKLES_CALLBACKS_H

#include <tox/tox.h>

void self_connection_status(__attribute__((unused)) Tox * tox,
        TOX_CONNECTION status,
        __attribute__((unused)) void *userData);


void friend_request(Tox *tox, const uint8_t *public_key,
        const uint8_t *message,
        __attribute__((unused)) size_t length,
        __attribute__((unused)) void * user_data);


void friend_on_off(Tox *tox, uint32_t friendNum,
        TOX_CONNECTION connection_status,
        __attribute__((unused)) void *user_data);


void file_recv(Tox *tox, uint32_t friendNum, uint32_t fileNum,
        uint32_t kind, __attribute__((unused)) uint64_t file_size,
        __attribute__((unused)) const uint8_t *filename,
        __attribute__((unused)) size_t filename_length,
        __attribute__((unused)) void *user_data);


void friend_message(Tox *tox, uint32_t friend_num,
        __attribute__((unused)) TOX_MESSAGE_TYPE type,
        const uint8_t *message, size_t length,
        __attribute__((unused)) void *user_data);

#endif
