#include "util.h"

#include "globals.h"

#include <sodium/utils.h>

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
    struct tm time_struct;
    const time_t curr_time = time(NULL);
    localtime_r(&curr_time, &time_struct);
    char timestr[50];

    strftime(timestr, sizeof(timestr), "[%b %d %T] ", &time_struct);
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

char * get_tox_ID(Tox * tox) {
    uint8_t address_bin[TOX_ADDRESS_SIZE];
    tox_self_get_address(tox, (uint8_t *) address_bin);
    size_t tox_ID_size = TOX_ADDRESS_SIZE*2 + 1; /* the +1 is for a terminating null byte */
    char * address_hex = malloc(tox_ID_size);
    sodium_bin2hex(address_hex, tox_ID_size, address_bin, sizeof(address_bin));
    address_hex[tox_ID_size-1] = '\0';
    return address_hex;
}

void bootstrap(Tox * tox) {
    struct bootstrap_node {
        const char * const ip;
        const uint16_t port;
        const char key_hex[TOX_PUBLIC_KEY_SIZE*2 + 1]; /* the +1 is for a terminating null byte */
        unsigned char key_bin[TOX_PUBLIC_KEY_SIZE];
    } nodes[10] = {
            {"nodes.tox.chat",  33445, "788237D34978D1D5BD822F0A5BEBD2C53C64CC31CD3149350EE27D4D9A2F9B6B", {0}},
            {"nodes.tox.chat",  33445, "6FC41E2BD381D37E9748FC0E0328CE086AF9598BECC8FEB7DDF2E440475F300E", {0}},
            {"130.133.110.14",  33445, "461FA3776EF0FA655F1A05477DF1B3B614F7D6B124F7DB1DD4FE3C08B03B640F", {0}},
            {"205.185.116.116", 33445, "A179B09749AC826FF01F37A9613F6B57118AE014D4196A0E1105A98F93A54702", {0}},
            {"163.172.136.118", 33445, "2C289F9F37C20D09DA83565588BF496FAB3764853FA38141817A72E3F18ACA0B", {0}},
            {"144.76.60.215",   33445, "04119E835DF3E78BACF0F84235B300546AF8B936F035185E2A8E9E0A67C8924F", {0}},
            {"23.226.230.47",   33445, "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074", {0}},
            {"178.21.112.187",  33445, "4B2C19E924972CB9B57732FB172F8A8604DE13EEDA2A6234E348983344B23057", {0}},
            {"195.154.119.113", 33445, "E398A69646B8CEACA9F0B84F553726C1C49270558C57DF5F3C368F05A7D71354", {0}},
            {"192.210.149.121", 33445, "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67", {0}}
    };

    TOX_ERR_BOOTSTRAP err;
    for (size_t i = 0; i < (sizeof(nodes)/sizeof(nodes[0])); i++) {
        logger("requesting nodes from %s:%d...", nodes[i].ip, nodes[i].port);
        sodium_hex2bin(nodes[i].key_bin, sizeof(nodes[i].key_bin),
                nodes[i].key_hex, sizeof(nodes[i].key_hex)-1,
                NULL, NULL, NULL);
        bool success = tox_bootstrap(tox, nodes[i].ip, nodes[i].port, nodes[i].key_bin, &err);
        if (! success) {
            if (err == TOX_ERR_BOOTSTRAP_BAD_HOST) {
                logger(" --> could not resolve host: %s", nodes[i].ip);
            } else {
                logger(" --> was not able to bootstrap: %s", nodes[i].ip);
            }
        }
        if (err != TOX_ERR_BOOTSTRAP_OK) {
            assert (! success);
            logger(" --> could not bootstrap, error code: %d", err);
        }
    }
}

/* ssssshhh I stole this from ToxBot, don't tell anyone.. */
void get_elapsed_time_str(char *buf, size_t bufsize, time_t secs) {
    long int minutes = (secs % 3600) / 60;
    long int hours = (secs / 3600) % 24;
    long int days = (secs / 3600) / 24;

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

    if (file_size < 0) {
        logger("file size is hosed: %s", strerror(errno));
        fclose(file);
        return TOX_ERR_NEW_LOAD_BAD_FORMAT;
    }
    size_t file_size_u = (size_t) file_size;

    uint8_t * save_data = calloc(file_size_u, sizeof(uint8_t));
    fread(save_data, sizeof(uint8_t), file_size_u, file);
    fclose(file);

    options->savedata_data = save_data;
    options->savedata_type = TOX_SAVEDATA_TYPE_TOX_SAVE;
    options->savedata_length = file_size_u;

    TOX_ERR_NEW err;
    *tox = tox_new(options, &err);
    free(save_data);

    return err;
}

void save_profile(Tox *tox) {
    /* the function set_data_path must be called before this one. */
    assert (data_filename != NULL);
    size_t save_size = tox_get_savedata_size(tox);
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
    static size_t status_number = 0;
    static const char * status;

    status_number = (status_number + 1) % mrprickles_nstatuses;
    status = mrprickles_statuses[status_number];

    logger("resetting info");

    tox_self_set_name(tox, (const uint8_t *) mrprickles_name, strlen(mrprickles_name), NULL);
    tox_self_set_status_message(tox, (const uint8_t *) status, strlen(status), NULL);

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
    size_t friend_count = tox_self_get_friend_list_size(tox);
    uint32_t * friends = calloc(friend_count, sizeof(uint32_t));

    tox_self_get_friend_list(tox, friends);

    uint32_t online_friend_count = 0u;
    for (size_t i = 0; i < friend_count; i++) {
        if (tox_friend_get_connection_status(tox, friends[i], NULL)
                != TOX_CONNECTION_NONE) {
            online_friend_count++;
        }
    }

    free(friends);
    return online_friend_count;
}
