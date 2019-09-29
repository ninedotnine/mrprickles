#pragma once

#include <tox/tox.h>
#include <tox/toxav.h>

#include <pthread.h>
#include <stdatomic.h>

#define GCC_UNUSED __attribute__((unused))

#define MRPRICKLES_VERSION "mrprickles version 0.3.3"

extern const char * const mrprickles_name;
extern const char * const mrprickles_statuses[];
extern const size_t mrprickles_nstatuses;
extern const char * const help_msg;

// reset name and status message every 6 hours
#define RESET_INFO_DELAY 21600

extern time_t last_info_change; // always a timestring
extern time_t start_time;
extern atomic_bool signal_exit;

extern const uint32_t audio_bitrate;
extern const uint32_t video_bitrate;

extern ToxAV *g_toxAV;
extern pthread_t main_thread;
