#include "util.h"

#include "globals.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void logger(const char * format, ...) {
    va_list ap;
    time_t timey = time(NULL);
    char timestr[50];

    strftime(timestr, sizeof(timestr), "[%b %d %T] ", localtime(&timey));
    fputs(timestr, stdout);

    va_start(ap, format);
    vprintf(format, ap);
    va_end(ap);

    putchar('\n');
}

void to_hex(char *out, uint8_t *in, int size) {
    /* stolen from uTox, merci */
    while (size--) {
        if (*in >> 4 < 0xA) {
            *out++ = '0' + (*in >> 4);
        } else {
            *out++ = 'A' + (*in >> 4) - 0xA;
        }

        if ((*in & 0xf) < 0xA) {
            *out++ = '0' + (*in & 0xF);
        } else {
            *out++ = 'A' + (*in & 0xF) - 0xA;
        }
        in++;
    }
}

/* ssssshhh I stole this from ToxBot, don't tell anyone.. */
void get_elapsed_time_str(char *buf, int bufsize, time_t secs) {
    long unsigned int minutes = (secs % 3600) / 60;
    long unsigned int hours = (secs / 3600) % 24;
    long unsigned int days = (secs / 3600) / 24;

    snprintf(buf, bufsize, "uptime: %lud %luh %lum", days, hours, minutes);
}

bool file_exists(const char *filename) {
    return access(filename, 0) != -1;
}

bool save_profile(Tox *tox) {
    uint32_t save_size = tox_get_savedata_size(tox);
    uint8_t save_data[save_size];

    tox_get_savedata(tox, save_data);

    FILE *file = fopen(data_filename, "wb");
    if (file) {
        fwrite(save_data, sizeof(uint8_t), save_size, file);
        fclose(file);
        return true;
    } else {
        logger("could not write data to disk");
        return false;
    }
}

TOX_ERR_NEW load_profile(Tox **tox, struct Tox_Options *options) {
    FILE *file = fopen(data_filename, "rb");

    if (! file) {
        // this should never happen...
        logger("could not open file %s", data_filename);
        return TOX_ERR_NEW_LOAD_BAD_FORMAT;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *save_data = calloc(file_size, sizeof(uint8_t));
    fread(save_data, sizeof(uint8_t), file_size, file);
    fclose(file);

    options->savedata_data = save_data;
    options->savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
    options->savedata_length = file_size;

    TOX_ERR_NEW err;
    *tox = tox_new(options, &err);
    free(save_data);

    return err;
}

void reset_info(Tox * tox) {
    static int status_number = 0;
    static const char * status;

    static_assert(sizeof(mrprickles_statuses)/sizeof(mrprickles_statuses[0]) == 2, "wrong statuses size");
    status_number = (status_number + 1) % (sizeof(mrprickles_statuses)/sizeof(mrprickles_statuses[0]));
    status = mrprickles_statuses[status_number];

    logger("resetting info");

    tox_self_set_name(tox, (uint8_t *) mrprickles_name, strlen(mrprickles_name), NULL);
    tox_self_set_status_message(tox, (uint8_t *) status, strlen(status), NULL);

    save_profile(tox);

    // set last_info_change to 0 to mean info hasn't been changed
    last_info_change = time(NULL);
}

// str is expected to point to an uninitialized pointer
void friend_name_from_num(uint8_t **str, Tox *tox, uint32_t friendNum) {
    TOX_ERR_FRIEND_QUERY err;
    size_t size = tox_friend_get_name_size(tox, friendNum, &err);
    if (err != TOX_ERR_FRIEND_QUERY_OK) {
        // what should we do?
        if (err == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND) {
            logger("no friend %u.", friendNum);
        } else {
            puts("how did this happen?");
        }
        return;
    }
    *str = calloc(sizeof(uint8_t), size+1); // a space at the end for a null byte. calloc initializes it to zero.
    tox_friend_get_name(tox, friendNum, *str, &err);
    if (err != TOX_ERR_FRIEND_QUERY_OK) {
        // what should we do?
        // this should never happen... yes?
        puts("a different problem occurred.");
    }
}

uint32_t get_online_friend_count(Tox *tox) {
    uint32_t online_friend_count = 0u;
    uint32_t friend_count = tox_self_get_friend_list_size(tox);
    uint32_t friends[friend_count];

    tox_self_get_friend_list(tox, friends);

    for (uint32_t i = 0; i < friend_count; i++) {
        if (tox_friend_get_connection_status(tox, friends[i], NULL)
                != TOX_CONNECTION_NONE) {
            online_friend_count++;
        }
    }

    return online_friend_count;
}