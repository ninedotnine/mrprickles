#include "globals.h"

const char * const mrprickles_name = "Mr. Prickles";

const char * const mrprickles_statuses[] = {
    "a humorously-named cactus from australia",
    "i am a robot pretending to be a cactus"
};
const size_t mrprickles_nstatuses = sizeof(mrprickles_statuses)
                                    / sizeof(mrprickles_statuses[0]);

const char * const help_msg = "list of commands:\ninfo: show stats.\ncallme: launch an audio call.\n"
    "videocallme: launch a video call.\nonline/away/busy: change my user status\nname: change my name\n"
    "status: change my status message";

time_t last_info_change; // always a timestring
time_t start_time;
atomic_bool signal_exit = false;

const uint32_t audio_bitrate = 48;
const uint32_t video_bitrate = 5000;

ToxAV *g_toxAV = NULL;
pthread_t main_thread;

