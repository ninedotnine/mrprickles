#include "callbacks.h"

#include "messaging.h"
#include "util.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void self_connection_status(GCC_UNUSED Tox * tox, TOX_CONNECTION status, GCC_UNUSED void *user_data) {
    switch (status) {
        case TOX_CONNECTION_NONE:
            logger("lost connection to the tox network");
            break;
        case TOX_CONNECTION_TCP:
            logger("connected to the tox network using TCP.");
            break;
        case TOX_CONNECTION_UDP:
            logger("connected to the tox network using UDP.");
            break;
        default:
            logger("this should absolutely not happen. status: %d", status);
    }
}

void friend_request(Tox *tox, const uint8_t *public_key, const uint8_t *message, GCC_UNUSED size_t length,
                    GCC_UNUSED void * user_data) {
    TOX_ERR_FRIEND_ADD err;
    tox_friend_add_norequest(tox, public_key, &err);
    logger("received friend request: %s", message);

    if (err != TOX_ERR_FRIEND_ADD_OK) {
        logger("could not add friend, error: %d", err);
    } else {
        logger("added to our friend list");
    }

    save_profile(tox);
}

void friend_on_off(Tox *tox, uint32_t friend_num, TOX_CONNECTION connection_status, GCC_UNUSED void *user_data) {
    uint8_t *name;
    friend_name_from_num(&name, tox, friend_num);
    if (connection_status == TOX_CONNECTION_NONE) {
        logger("friend %u (%s) went offline", friend_num, name);
    } else {
        logger("friend %u (%s) came online", friend_num, name);
    }
    free(name);
}

void file_recv(Tox *tox, uint32_t friend_num, uint32_t file_num, uint32_t kind, GCC_UNUSED uint64_t file_size,
                GCC_UNUSED const uint8_t *filename, GCC_UNUSED size_t filename_length, GCC_UNUSED void *user_data) {
    if (kind == TOX_FILE_KIND_AVATAR) {
        return;
    }

    tox_file_control(tox, friend_num, file_num, TOX_FILE_CONTROL_CANCEL, NULL);

    const char *msg = "i don't want your dumb file.";
    tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
            (uint8_t*) msg, strlen(msg), NULL);
}

void friend_message(Tox *tox, uint32_t friend_num, GCC_UNUSED TOX_MESSAGE_TYPE type,
                    const uint8_t *message, size_t length, GCC_UNUSED void *user_data) {
    if (type == TOX_MESSAGE_TYPE_ACTION) {
        char* reply = ":^O";
        tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL, (uint8_t *) reply, strlen(reply), NULL);
        return;
    }
    assert (type == TOX_MESSAGE_TYPE_NORMAL);

    uint8_t *name;
    friend_name_from_num(&name, tox, friend_num);
    logger("friend %u (%s) says: \033[1m%s\033[0m", friend_num, name, message);
    free(name);

    // the message was passed to us without a terminating null byte. the null byte here is provided by calloc
    char * dest_msg = calloc(sizeof(char), length+1);
    if (NULL == dest_msg) {
        logger("oh no, couldn't allocate memory.");
        return;
    }
    memcpy(dest_msg, message, length);
    reply_friend_message(tox, friend_num, dest_msg, length);
    free(dest_msg);
}
