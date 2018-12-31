#ifndef MRPRICKLES_UTIL_H
#define MRPRICKLES_UTIL_H

#include <tox/tox.h>

#include <time.h>

void logger(const char * format, ...);

void to_hex(char *out, uint8_t *in, int size);

void get_elapsed_time_str(char *buf, int bufsize, time_t secs);

bool file_exists(const char *filename);

bool save_profile(Tox *tox);

TOX_ERR_NEW load_profile(Tox **tox, struct Tox_Options *options);

void reset_info(Tox * tox);

// str is expected to point to an uninitialized pointer
void friend_name_from_num(uint8_t **str, Tox *tox, uint32_t friendNum);

uint32_t get_online_friend_count(Tox *tox);

#endif