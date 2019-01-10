#include "av_callbacks.h"
#include "callbacks.h"
#include "globals.h"
#include "messaging.h"
#include "util.h"

#include <sodium/utils.h>

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

void bootstrap(void) {
    struct bootstrap_node {
        const char * const ip;
        const uint16_t port;
        const char key_hex[TOX_PUBLIC_KEY_SIZE*2 + 1];
        unsigned char key_bin[TOX_PUBLIC_KEY_SIZE];
    } nodes[10] = {
            {"nodes.tox.chat",  33445,
                "788237D34978D1D5BD822F0A5BEBD2C53C64CC31CD3149350EE27D4D9A2F9B6B", {0}},
            {"nodes.tox.chat", 33445,
                "6FC41E2BD381D37E9748FC0E0328CE086AF9598BECC8FEB7DDF2E440475F300E", {0}},
            {"130.133.110.14", 33445,
                "461FA3776EF0FA655F1A05477DF1B3B614F7D6B124F7DB1DD4FE3C08B03B640F", {0}},
            {"205.185.116.116", 33445,
                "A179B09749AC826FF01F37A9613F6B57118AE014D4196A0E1105A98F93A54702", {0}},
            {"163.172.136.118", 33445,
                "2C289F9F37C20D09DA83565588BF496FAB3764853FA38141817A72E3F18ACA0B", {0}},
            {"144.76.60.215",   33445,
                "04119E835DF3E78BACF0F84235B300546AF8B936F035185E2A8E9E0A67C8924F", {0}},
            {"23.226.230.47",   33445,
                "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074", {0}},
            {"178.21.112.187",  33445,
                "4B2C19E924972CB9B57732FB172F8A8604DE13EEDA2A6234E348983344B23057", {0}},
            {"195.154.119.113", 33445,
                "E398A69646B8CEACA9F0B84F553726C1C49270558C57DF5F3C368F05A7D71354", {0}},
            {"192.210.149.121", 33445,
                "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67", {0}}
    };

    TOX_ERR_BOOTSTRAP err2;

    for (size_t i = 0; i < (sizeof(nodes)/sizeof(nodes[0])); i++) {
        logger("requesting nodes from %s:%d...", nodes[i].ip, nodes[i].port);
        sodium_hex2bin(nodes[i].key_bin, sizeof(nodes[i].key_bin),
                nodes[i].key_hex, sizeof(nodes[i].key_hex)-1,
                NULL, NULL, NULL);
        bool success = tox_bootstrap(g_tox, nodes[i].ip, nodes[i].port,
                nodes[i].key_bin, &err2);
        if (! success) {
            if (err2 == TOX_ERR_BOOTSTRAP_BAD_HOST) {
                logger(" --> could not resolve host: %s", nodes[i].ip);
            } else {
                logger(" --> was not able to bootstrap: %s", nodes[i].ip);
            }
        }
        if (err2 != TOX_ERR_BOOTSTRAP_OK) {
            assert (! success);
            logger(" --> could not bootstrap, error code: %d", err2);
        }
    }
}

static void * run_toxav(void * arg) {
    ToxAV * toxav = (ToxAV *) arg;
    assert (toxav != NULL);

    for (long long interval; true; usleep(interval)) {
        toxav_iterate(toxav);
        interval = toxav_iteration_interval(toxav) * 1000L; // microseconds
    }
    return NULL;
}

static void * run_tox(void * arg) {
    Tox * tox = (Tox *) arg;
    assert (tox != NULL);

    time_t curr_time;

    for (long long interval; true; usleep(interval)) {
        tox_iterate(tox, NULL);

        curr_time = time(NULL);
        if (curr_time - last_info_change > RESET_INFO_DELAY) {
            reset_info(tox);
        }

        interval = tox_iteration_interval(tox) * 1000L; // in microseconds
    }
    return NULL;
}

static void handle_signal(int sig) {
    if (sig == SIGINT) {
        logger("received SIGINT");
    } else {
        assert(sig == SIGTERM);
        logger("received SIGTERM");
    }
    signal_exit = true;
}

int main(void) {
    start_time = time(NULL);
    main_thread = pthread_self();

    logger(MRPRICKLES_VERSION);

    TOX_ERR_NEW err = TOX_ERR_NEW_OK;
    struct Tox_Options options;
    tox_options_default(&options);

    char * data_filename = set_data_path();

    if (file_exists(data_filename)) {
        err = load_profile(&g_tox, &options, data_filename);
        if (err == TOX_ERR_NEW_OK) {
            logger("loaded data from %s", data_filename);
        } else {
            logger("failed to load data from disk: error code %d", err);
            exit(EXIT_FAILURE);
        }
    } else {
        puts("creating a new profile");

        g_tox = tox_new(&options, &err);

        if (err != TOX_ERR_NEW_OK) {
            logger("error at tox_new, error: %d", err);
            exit(EXIT_FAILURE);
        }

    }
    reset_info(g_tox);

    tox_callback_self_connection_status(g_tox, self_connection_status);
    tox_callback_friend_connection_status(g_tox, friend_on_off);
    tox_callback_friend_request(g_tox, friend_request);
    tox_callback_friend_message(g_tox, friend_message);
    tox_callback_file_recv(g_tox, file_recv);

    uint8_t address_bin[TOX_ADDRESS_SIZE];
    tox_self_get_address(g_tox, (uint8_t *)address_bin);
    char address_hex[TOX_ADDRESS_SIZE * 2 + 1];
    sodium_bin2hex(address_hex, sizeof(address_hex), address_bin,
            sizeof(address_bin));

    printf("%s\n", address_hex);

    bootstrap();

    TOXAV_ERR_NEW err3;
    g_toxAV = toxav_new(g_tox, &err3);
    if (err3 != TOXAV_ERR_NEW_OK) {
        logger("error at toxav_new: %d", err3);
        exit(EXIT_FAILURE);
    }

    toxav_callback_call(g_toxAV, call, NULL);
    toxav_callback_call_state(g_toxAV, call_state, NULL);
    toxav_callback_audio_receive_frame(g_toxAV, audio_receive_frame, NULL);
    toxav_callback_video_receive_frame(g_toxAV, video_receive_frame, NULL);

    /* set up signal handling. */
    struct sigaction new_action;
    sigemptyset(&new_action.sa_mask);
    new_action.sa_handler = handle_signal;
    new_action.sa_flags = 0;
    sigaction(SIGINT, &new_action, NULL);
    sigaction(SIGTERM, &new_action, NULL);

    pthread_t tox_thread, toxav_thread;
    pthread_create(&tox_thread, NULL, &run_tox, g_tox);
    pthread_create(&toxav_thread, NULL, &run_toxav, g_toxAV);

    while (!signal_exit) {
        /* as i understand it, the call to sleep will be interrupted by sigint
           anyway. there is no need to waste cpu resources here...  */
        pause();
    }

    logger("killing tox and saving profile...");
    sleep(1); // a bit of time for messages to finish

    int status_av_thread = pthread_cancel(toxav_thread);
    int status_thread = pthread_cancel(tox_thread);

    if (status_av_thread != 0 || status_thread != 0) {
        logger("oh man, threads didn't want to be cancelled, this is bad");
        sleep(2);
        exit(EXIT_FAILURE);
    }

    /* wait for threads to exit */
    status_av_thread = pthread_join(toxav_thread, NULL);
    status_thread = pthread_join(tox_thread, NULL);

    assert (0 == status_av_thread);
    assert (0 == status_thread);

    save_profile(g_tox);
    free(data_filename);

    toxav_kill(g_toxAV);
    tox_kill(g_tox);

    return 0;
}
