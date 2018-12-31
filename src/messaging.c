#define _GNU_SOURCE

#include "messaging.h"

#include "globals.h"
#include "util.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void send_info_message(Tox* tox, uint32_t friend_num) {
    char msg[TOX_MAX_MESSAGE_LENGTH];

    char hostname[100];
    static_assert(TOX_MAX_MESSAGE_LENGTH - sizeof(hostname) > 50, "TOX_MAX_MESSAGE_LENGTH is very small.");
    if (gethostname(hostname, sizeof(hostname))) {
        logger("unable to get hostname");
    } else {
        snprintf(msg, sizeof(msg), "%s on %s", MRPRICKLES_VERSION, hostname);
        tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
            (uint8_t *) msg, strlen(msg), NULL);
    }

    time_t cur_time = time(NULL);
    get_elapsed_time_str(msg, sizeof(msg), cur_time-start_time);
    tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
            (uint8_t *) msg, strlen(msg), NULL);

    snprintf(msg, sizeof(msg), "friends: %zu (%u online)",
            tox_self_get_friend_list_size(tox),
            get_online_friend_count(tox));
    tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
            (uint8_t *) msg, strlen(msg), NULL);
}

static void send_friends_list_message(Tox* tox, uint32_t friend_num) {
    size_t friendCount = tox_self_get_friend_list_size(tox);
    uint32_t friendList[friendCount];
    tox_self_get_friend_list(tox, friendList);

    for (uint32_t i = 0; i < friendCount; i++) {
        TOX_ERR_FRIEND_QUERY err;
        size_t nameSize = tox_friend_get_name_size(tox, i, &err);
        if (err != TOX_ERR_FRIEND_QUERY_OK) {
            if (err == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND) {
                logger("no friend %u.", i);
            } else {
                puts("wait, what? this shouldn't happen.");
            }
            continue;
        }

        uint8_t *friend_name;
        friend_name_from_num(&friend_name, tox, i);

        // what is the status of an offline friend?
        TOX_USER_STATUS status = tox_friend_get_status(tox, i, &err);
        if (err != TOX_ERR_FRIEND_QUERY_OK) {
            puts("could not get friends status");
            continue;
        }

        // enough space for name, 3-digit number, status
        char msg[nameSize+17];

        if (tox_friend_get_connection_status(tox, friendList[i], NULL)
                == TOX_CONNECTION_NONE) {
            snprintf(msg, sizeof(msg), "%u: %s (offline)", i, friend_name);
        } else {
            if (status == TOX_USER_STATUS_NONE) {
                snprintf(msg, sizeof(msg), "%u: %s (online)", i, friend_name);
            } else if (status == TOX_USER_STATUS_AWAY) {
                snprintf(msg, sizeof(msg), "%u: %s (away)", i, friend_name);
            } else if (status == TOX_USER_STATUS_BUSY) {
                snprintf(msg, sizeof(msg), "%u: %s (busy)", i, friend_name);
            }
        }

        free(friend_name);
        tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                (uint8_t *) msg, strlen(msg), NULL);
    }
}

static void send_keys_message(Tox* tox, uint32_t friend_num) {
    size_t friendCount = tox_self_get_friend_list_size(tox);
    uint32_t friendList[friendCount];
    tox_self_get_friend_list(tox, friendList);

    logger("listing public key for each friend.");
    for (uint32_t i = 0; i < friendCount; i++) {
        TOX_ERR_FRIEND_QUERY err;
        size_t nameSize = tox_friend_get_name_size(tox, i, &err);
        if (err != TOX_ERR_FRIEND_QUERY_OK) {
            if (err == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND) {
                logger("no friend %u.", i);
            } else {
                puts("wait, what? this shouldn't happen.");
            }
            continue;
        }

        uint8_t *friend_name;
        friend_name_from_num(&friend_name, tox, i);

        // enough space for a number, the name, the pubkey
        char msg[10+nameSize+(2*TOX_PUBLIC_KEY_SIZE)];

        TOX_ERR_FRIEND_GET_PUBLIC_KEY err2;
        uint8_t pubkey_bin[TOX_PUBLIC_KEY_SIZE];
        bool ok = tox_friend_get_public_key(tox, i, pubkey_bin, &err2);
        if ((! ok) || (err2 != TOX_ERR_FRIEND_GET_PUBLIC_KEY_OK)) {
            logger("can't get friend %u's key.", i);
            continue;
        }

        const uint8_t hex_length = (TOX_PUBLIC_KEY_SIZE * 2) + 1;
        char pubkey_hex[hex_length];
        to_hex(pubkey_hex, pubkey_bin, TOX_PUBLIC_KEY_SIZE);
        pubkey_hex[hex_length-1] = '\0';
        snprintf(msg, sizeof(msg), "%u: %s %s", i, friend_name, pubkey_hex);
        puts(msg);

        tox_friend_send_message(tox, friend_num,
                                TOX_MESSAGE_TYPE_NORMAL,
                                (uint8_t *) msg, strlen(msg), NULL);
        free(friend_name);
    }
}

void reply_friend_message(Tox *tox, uint32_t friend_num, char *dest_msg, size_t length) {
    if (!strncmp("info", dest_msg, 4)) {
        send_info_message(tox, friend_num);
    } else if (!strncmp("friends", dest_msg, 7)) {
        send_friends_list_message(tox, friend_num);
    } else if (!strcmp("keys", dest_msg)) {
        if (friend_num == 0) { /* friend 0 is considered the admin. */
            send_keys_message(tox, friend_num);
        } else {
            char *msg = "i'll show you mine if you show me yours.";
            tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                (uint8_t *) msg, strlen(msg), NULL);
        }
    } else if (!strncmp("name ", dest_msg, 5) && sizeof(dest_msg) > 5) {
        char * new_name = dest_msg + 5;
        tox_self_set_name(tox, (uint8_t *) new_name, strlen(new_name), NULL);
        last_info_change = time(NULL);
    } else if (!strncmp("status ", dest_msg, 7) && sizeof(dest_msg) > 7) {
        char * new_status = dest_msg + 7;
        tox_self_set_status_message(tox, (uint8_t *) new_status,
                strlen(new_status),NULL);
        last_info_change = time(NULL);
    } else if (!strncmp("busy", dest_msg, 4)) {
        tox_self_set_status(tox, TOX_USER_STATUS_BUSY);
        const char *msg = "leave me alone; i'm busy.";
        tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                (uint8_t *) msg, strlen(msg), NULL);
    } else if (!strncmp("away", dest_msg, 4)) {
        tox_self_set_status(tox, TOX_USER_STATUS_AWAY);
        const char *msg = "i'm not here right now.";
        tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                (uint8_t *) msg, strlen(msg), NULL);
    } else if (!strncmp("online", dest_msg, 6)) {
        tox_self_set_status(tox, TOX_USER_STATUS_NONE);
        const char *msg = "sup? sup brah?";
        tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                (uint8_t *) msg, strlen(msg), NULL);
    } else if (!strncmp("reset", dest_msg, 5)) {
        reset_info(tox);
    } else if (!strncmp("callme", dest_msg, 6)) {
        toxav_call(g_toxAV, friend_num, audio_bitrate, 0, NULL);
    } else if (!strcmp ("videocallme", dest_msg)) {
        toxav_call(g_toxAV, friend_num, audio_bitrate, video_bitrate, NULL);
    } else if (!strncmp ("help", dest_msg, 4)) {
        /* Send usage instructions in new message. */
        tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                (uint8_t*) help_msg, strlen (help_msg), NULL);
    } else if (!strcmp ("suicide", dest_msg)) {
        if (friend_num == 0) { /* friend 0 is considered the admin. */
            const char *msg = "so it has come to this...";
            tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                    (uint8_t *) msg, strlen(msg), NULL);
            signal_exit = true;
            logger("sending SIGINT");
            int success = pthread_kill(main_thread, SIGINT);
            assert (success == 0);
        } else {
            const char *msg = "...?";
            tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                    (uint8_t *) msg, strlen(msg), NULL);
        }
    } else {
        /* Just repeat what has been said like the nymph Echo. */
        tox_friend_send_message(tox, friend_num, TOX_MESSAGE_TYPE_NORMAL,
                (uint8_t*) dest_msg, length, NULL);
    }
}
