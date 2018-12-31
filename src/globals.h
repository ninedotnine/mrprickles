#pragma once

#include <tox/tox.h>
#include <tox/toxav.h>

#include <pthread.h>

#define GCC_UNUSED __attribute__((unused))

#define MRPRICKLES_VERSION "mrprickles version 0.3.0"

// reset name and status message every 6 hours
#define RESET_INFO_DELAY 21600

extern const char * const mrprickles_name;
extern const char * const mrprickles_statuses[2];

extern time_t last_info_change; // always a timestring
extern time_t start_time;
extern bool signal_exit;

extern const int32_t audio_bitrate;
extern const int32_t video_bitrate;

extern Tox *g_tox;
extern ToxAV *g_toxAV;
extern pthread_t main_thread;

extern const char *help_msg;
