#pragma once

#include "globals.h"

#include <tox/tox.h>

void self_connection_status(GCC_UNUSED Tox * tox, TOX_CONNECTION status, GCC_UNUSED void *userData);


void friend_request(Tox *tox, const uint8_t *public_key, const uint8_t *message,
                    GCC_UNUSED size_t length, GCC_UNUSED void * user_data);


void friend_on_off(Tox *tox, uint32_t friendNum, TOX_CONNECTION connection_status, GCC_UNUSED void *user_data);


void file_recv(Tox *tox, uint32_t friendNum, uint32_t fileNum, uint32_t kind, GCC_UNUSED uint64_t file_size,
                GCC_UNUSED const uint8_t *filename, GCC_UNUSED size_t filename_length, GCC_UNUSED void *user_data);


void friend_message(Tox *tox, uint32_t friend_num, GCC_UNUSED TOX_MESSAGE_TYPE type,
                    const uint8_t *message, size_t length, GCC_UNUSED void *user_data);
