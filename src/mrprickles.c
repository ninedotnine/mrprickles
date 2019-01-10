#include "av_callbacks.h"
#include "callbacks.h"
#include "globals.h"
#include "messaging.h"
#include "util.h"

#include <assert.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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

    /* register tox callbacks. */
    tox_callback_self_connection_status(g_tox, self_connection_status);
    tox_callback_friend_connection_status(g_tox, friend_on_off);
    tox_callback_friend_request(g_tox, friend_request);
    tox_callback_friend_message(g_tox, friend_message);
    tox_callback_file_recv(g_tox, file_recv);

    /* output my tox ID. */
    char * tox_id = get_tox_ID(g_tox);
    printf("%s\n", tox_id);
    free(tox_id);

    /* start it up. */
    bootstrap(g_tox);

    /* create toxav and register callbacks. */
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

    /* start the threads and chill out for a while. */
    pthread_t tox_thread, toxav_thread;
    pthread_create(&tox_thread, NULL, &run_tox, g_tox);
    pthread_create(&toxav_thread, NULL, &run_toxav, g_toxAV);

    while (!signal_exit) {
        pause();
    }

    logger("killing tox and saving profile...");
    sleep(1); // a bit of time for messages to finish

    /* start packing up. */
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
