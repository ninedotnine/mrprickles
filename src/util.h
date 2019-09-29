#pragma once

#include <tox/tox.h>

#include <time.h>

void logger(const char * format, ...);

void to_hex(char *out, uint8_t *in, int size);

char * get_tox_ID(Tox * tox);

void bootstrap(Tox * tox);

void get_elapsed_time_str(char *buf, size_t bufsize, time_t secs);

bool file_exists(const char *filename);

char * set_data_path(void);

void save_profile(Tox *tox);

TOX_ERR_NEW load_profile(Tox **tox, struct Tox_Options *options, const char * const data_filename);

void reset_info(Tox * tox);

// str is expected to point to an uninitialized pointer
void friend_name_from_num(uint8_t **str, Tox *tox, uint32_t friend_num);

uint32_t get_online_friend_count(Tox *tox);
