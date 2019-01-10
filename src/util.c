#include "util.h"

#include "globals.h"

#include <assert.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

static const char * data_filename;

char * set_data_path(void) {
    const char * home_dir;
    if ((home_dir = getenv("HOME")) == NULL) {
        struct passwd * pwuid = getpwuid(getuid());
        if (! pwuid) {
            logger("unable to find home directory; saving to a temporary location.");
            home_dir = "/tmp";
        } else {
            home_dir = pwuid->pw_dir;
        }
    }
    assert(home_dir != NULL);

    char * cache_dir;
    int asprintf_success = asprintf(&cache_dir, "%s/.cache", home_dir);
    if (asprintf_success == -1) {
        logger("problem with asprintf, possible memory shortage.");
        exit(EXIT_FAILURE);
    }
    assert(cache_dir != NULL);

    const int cache_dir_perms = S_IRUSR | S_IWUSR | S_IXUSR | S_IRGRP | S_IXGRP | S_IROTH | S_IXOTH; /* 755 */
    const int cache_dir_exists = mkdir(cache_dir, cache_dir_perms);
    if (cache_dir_exists != 0 && errno != EEXIST) {
        logger("problem creating cache directory.");
        exit(EXIT_FAILURE);
    }

    char * file_name;
    asprintf_success = asprintf(&file_name, "%s/tox_mrprickles", cache_dir);
    free(cache_dir);
    if (asprintf_success == -1) {
        logger("problem with asprintf, possible memory shortage, value: %d", asprintf_success);
        exit(EXIT_FAILURE);
    }

    /* store this name at the top level of this file for use by the save_profile routine.
       this is unfortunate, but the other options are:
           * store the filename in a global variable (globals.c)
           * store it as a static local variable in both routines
           * save_profile could ask for it as a parameter. a pointer to it would need to be passed to every callback
             that wants to use it, cast to a (void*).
       although unsavoury, this seems the best option. */
    assert(file_name != NULL);
    data_filename = file_name;
    return file_name;
}

TOX_ERR_NEW load_profile(Tox **tox, struct Tox_Options *options, const char * const filename) {
    FILE *file = fopen(filename, "rb");

    if (! file) {
        // this should never happen...
        logger("could not open file %s", filename);
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

void save_profile(Tox *tox) {
    /* the function set_data_path must be called before this one. */
    assert (data_filename != NULL);
    uint32_t save_size = tox_get_savedata_size(tox);
    uint8_t* save_data = calloc(save_size, sizeof(uint8_t));
    tox_get_savedata(tox, save_data);

    FILE *file = fopen(data_filename, "wb");
    if (file) {
        fwrite(save_data, sizeof(uint8_t), save_size, file);
        fclose(file);
        logger("data written.");
    } else {
        logger("could not write data");
    }
    free(save_data);
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

    // after resetting info, set last_info_change to current time
    last_info_change = time(NULL);
}

// str is expected to point to an uninitialized pointer
void friend_name_from_num(uint8_t **str, Tox *tox, uint32_t friend_num) {
    TOX_ERR_FRIEND_QUERY err;
    size_t size = tox_friend_get_name_size(tox, friend_num, &err);
    if (err != TOX_ERR_FRIEND_QUERY_OK) {
        // what should we do?
        if (err == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND) {
            logger("no friend %u.", friend_num);
        } else {
            puts("how did this happen?");
        }
        return;
    }
    *str = calloc(sizeof(uint8_t), size+1); // a space at the end for a null byte. calloc initializes it to zero.
    tox_friend_get_name(tox, friend_num, *str, &err);
    if (err != TOX_ERR_FRIEND_QUERY_OK) {
        // what should we do?
        // this should never happen... yes?
        puts("a different problem occurred.");
    }
}

uint32_t get_online_friend_count(Tox *tox) {
    uint32_t online_friend_count = 0u;
    uint32_t friend_count = tox_self_get_friend_list_size(tox);
    uint32_t * friends = calloc(friend_count, sizeof(uint32_t));

    tox_self_get_friend_list(tox, friends);

    for (uint32_t i = 0; i < friend_count; i++) {
        if (tox_friend_get_connection_status(tox, friends[i], NULL)
                != TOX_CONNECTION_NONE) {
            online_friend_count++;
        }
    }

    free(friends);
    return online_friend_count;
}
