#define _GNU_SOURCE
#include <pthread.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pwd.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <stdbool.h>

#include <tox/tox.h>
#include <tox/toxav.h>
#include <sodium.h>

// static uint64_t last_purge;
static uint64_t last_info_change;
static uint64_t start_time;
static bool signal_exit = false;

static const int32_t audio_bitrate = 48;
static const int32_t video_bitrate = 5000;
static char *data_filename;

static Tox *g_tox = NULL;
static ToxAV *g_toxAV = NULL;

typedef struct DHT_node {
    const char *ip;
    uint16_t port;
    const char key_hex[TOX_PUBLIC_KEY_SIZE*2 + 1];
    unsigned char key_bin[TOX_PUBLIC_KEY_SIZE];
} DHT_node;

static const char *help_msg = "list of commands:\ninfo: show stats.\ncallme: launch an audio call.\nvideocallme: launch a video call.\nonline/away/busy: change my user status\nname: change my name\nstatus: change my status message";

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
		printf("Could not write data to disk\n");
		return false;
	}
}

void reset_info(Tox * tox) {
    const char *name = "Mr. Prickles";
    const char *status = "a humorously-named cactus from australia";

    tox_self_set_name(tox, (uint8_t *)name, strlen(name), NULL);
    tox_self_set_status_message(tox, (uint8_t *)status, strlen(status), NULL);

    save_profile(tox);
}

static void * run_toxav(void * arg) {
	ToxAV * toxav = (ToxAV *) arg;

	for (long long interval; true; usleep(interval)) {
		toxav_iterate(toxav);
		interval = toxav_iteration_interval(toxav) * 1000L; // microseconds
	}
	return NULL;
}

static void * run_tox(void * arg) {
	Tox * tox = (Tox *) arg;
    uint64_t curr_time;

    for (long long interval; true; usleep(interval)) {
		tox_iterate(tox, NULL);

        // reset name and status message every 60 minutes 
		curr_time = time(NULL);
		if (last_info_change == 0 || curr_time - last_info_change > 3600) {
			reset_info(tox);
		}

		interval = tox_iteration_interval(tox) * 1000L; // in microseconds
	}
	return NULL;
}

/* ssssshhh I stole this from ToxBot, don't tell anyone.. */
static void get_elapsed_time_str(char *buf, int bufsize, uint64_t secs) {
	long unsigned int minutes = (secs % 3600) / 60;
	long unsigned int hours = (secs / 3600) % 24;
	long unsigned int days = (secs / 3600) / 24;

	snprintf(buf, bufsize, "%lud %luh %lum", days, hours, minutes);
}

bool file_exists(const char *filename) {
	return access(filename, 0) != -1;
}

TOX_ERR_NEW load_profile(Tox **tox, struct Tox_Options *options) {
	FILE *file = fopen(data_filename, "rb");

	if (! file) {
        // this should never happen... 
        printf("could not open file %s\n", data_filename);
        return TOX_ERR_NEW_LOAD_BAD_FORMAT;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    uint8_t *save_data = (uint8_t *)malloc(file_size * sizeof(uint8_t));
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

void self_connection_status(Tox *tox, TOX_CONNECTION status, void *userData) {
	if (status == TOX_CONNECTION_NONE) {
		printf("Lost connection to the tox network\n");
	} else {
		printf("Connected to the tox network, status: %d\n", status);
	}
}

void friend_request(Tox *tox, const uint8_t *public_key, 
                    const uint8_t *message, size_t length, void *user_data) {
	TOX_ERR_FRIEND_ADD err;
	tox_friend_add_norequest(tox, public_key, &err);

	if (err != TOX_ERR_FRIEND_ADD_OK) {
		printf("Could not add friend, error: %d\n", err);
	} else {
		printf("Added to our friend list\n");
	}

	save_profile(tox);
}

// str is expected to point to an uninitialized pointer
void friend_name_from_num(uint8_t **str, Tox *tox, uint32_t friendNum) {
    TOX_ERR_FRIEND_QUERY err;
    size_t size = tox_friend_get_name_size(tox, friendNum, &err);
    if (err != TOX_ERR_FRIEND_QUERY_OK) {
        // what should we do?
        puts("a problem occurred.");
        return;
    }
    *str = calloc(sizeof(uint8_t), size);
    tox_friend_get_name(tox, friendNum, *str, &err);
    if (err != TOX_ERR_FRIEND_QUERY_OK) {
        // what should we do?
        // this should never happen... yes?
        puts("a different problem occurred.");
    }
}

void friend_on_off(Tox *tox, uint32_t friendNum, 
                   TOX_CONNECTION connection_status, void *user_data) {
    uint8_t *name;
    friend_name_from_num(&name, tox, friendNum);
    if (connection_status == TOX_CONNECTION_NONE) {
        printf("friend %d (%s) went offline\n", friendNum, name);
    } else {
        printf("friend %d (%s) came online\n", friendNum, name);
    }
}

void friend_message(Tox *tox, uint32_t friendNum, TOX_MESSAGE_TYPE type, 
                    const uint8_t *message, size_t length, void *user_data) {
    uint8_t *name;
    friend_name_from_num(&name, tox, friendNum);
    printf("friend %d (%s) says: %s\n", friendNum, name, message);
    // dan: what is the point of dest_msg ? get rid of it?
	char dest_msg[length + 1];
	dest_msg[length] = '\0';
	memcpy(dest_msg, message, length);

	if (!strcmp("info", dest_msg)) {
		char time_msg[TOX_MAX_MESSAGE_LENGTH];
		char time_str[64];
		uint64_t cur_time = time(NULL);

		get_elapsed_time_str(time_str, sizeof(time_str), cur_time-start_time);
		snprintf(time_msg, sizeof(time_msg), "uptime: %s", time_str);
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL,
                                (uint8_t *)time_msg, strlen(time_msg), NULL);
		char friend_msg[100];
		snprintf(friend_msg, sizeof(friend_msg), "friends: %zu (%d online)",
                 tox_self_get_friend_list_size(tox), 
                 get_online_friend_count(tox));
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL, 
                                (uint8_t *)friend_msg,strlen(friend_msg),NULL);

		const char *info_msg = "If you're experiencing issues, well..."; 
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL,
                                (uint8_t *)info_msg, strlen(info_msg), NULL);


    } else if (!strcmp("friends", dest_msg)) {
        size_t friendCount = tox_self_get_friend_list_size(tox);
        uint32_t friendList[friendCount];
        tox_self_get_friend_list(tox, friendList);

        for (uint32_t i = 0; i < friendCount; i++) {
            TOX_ERR_FRIEND_QUERY err;
            size_t nameSize = tox_friend_get_name_size(tox, i, &err);
            if (err != TOX_ERR_FRIEND_QUERY_OK) {
                if (err == TOX_ERR_FRIEND_QUERY_FRIEND_NOT_FOUND) {
                    printf("no friend %u.", i);
                } else {
                    puts("wait, what? this shouldn't happen.");
                }
                continue;
            }

            uint8_t *name = calloc(sizeof(uint8_t), nameSize);
            tox_friend_get_name(tox, i, name, &err);
            if (err != TOX_ERR_FRIEND_QUERY_OK) {
                puts("could not get friend name");
                continue;
            }

            size_t friendNameSize = tox_friend_get_name_size(tox, i, &err);
            if (err != TOX_ERR_FRIEND_QUERY_OK) {
                puts("could not get name size");
                continue;
            }

            // what is the status of an offline friend?
            TOX_USER_STATUS status = tox_friend_get_status(tox, i, &err);
            if (err != TOX_ERR_FRIEND_QUERY_OK) {
                puts("could not get friends status");
                continue;
            }

            // enough space for name, 3-digit number, status
            char msg[friendNameSize+17]; 

            if (tox_friend_get_connection_status(tox, friendList[i], NULL) 
                    == TOX_CONNECTION_NONE) {
                snprintf(msg, sizeof(msg), "%u: %s (offline)", i, name);
            } else {
                if (status == TOX_USER_STATUS_NONE) {
                    snprintf(msg, sizeof(msg), "%u: %s (online)", i, name);
                } else if (status == TOX_USER_STATUS_AWAY) {
                    snprintf(msg, sizeof(msg), "%u: %s (away)", i, name);
                } else if (status == TOX_USER_STATUS_BUSY) {
                    snprintf(msg, sizeof(msg), "%u: %s (busy)", i, name);
                }
            }

            tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL, 
                                    (uint8_t *) msg, strlen(msg), NULL);
        }

    } else if (!strcmp("keys", dest_msg)) {
		const char *msg = "i'll show you mine if you show me yours."; 
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL,
                                (uint8_t *) msg, strlen(msg), NULL);
	} else if (!strncmp("name ", dest_msg, 5) && sizeof(dest_msg) > 5) {
        char * new_name = dest_msg + 5;
        tox_self_set_name(tox, (uint8_t *) new_name, strlen(new_name), NULL);
        last_info_change = time(NULL);
	} else if (!strncmp("status ", dest_msg, 7) && sizeof(dest_msg) > 7) {
        char * new_status = dest_msg + 7;
        tox_self_set_status_message(tox, (uint8_t *) new_status,
                                    strlen(new_status),NULL);
        last_info_change = time(NULL);
	} else if (!strcmp("busy", dest_msg)) {
        tox_self_set_status(tox, TOX_USER_STATUS_BUSY);
		const char *msg = "leave me alone; i'm busy."; 
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL, 
                                (uint8_t *) msg, strlen(msg), NULL);
	} else if (!strcmp("away", dest_msg)) {
        tox_self_set_status(tox, TOX_USER_STATUS_AWAY);
		const char *msg = "i'm not here right now."; 
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL, 
                                (uint8_t *) msg, strlen(msg), NULL);
	} else if (!strcmp("online", dest_msg)) {
        tox_self_set_status(tox, TOX_USER_STATUS_NONE);
		const char *msg = "sup? sup brah?"; 
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL, 
                                (uint8_t *) msg, strlen(msg), NULL);
	} else if (!strcmp("callme", dest_msg)) {
		toxav_call(g_toxAV, friendNum, audio_bitrate, 0, NULL);
	} else if (!strcmp ("videocallme", dest_msg)) {
		toxav_call (g_toxAV, friendNum, audio_bitrate, video_bitrate, 
                    NULL);
	} else if (!strcmp ("help", dest_msg)) {
		/* Send usage instructions in new message. */
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL, 
                               (uint8_t*) help_msg, strlen (help_msg), NULL);
	} else {
		/* Just repeat what has been said like the nymph Echo. */
		tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL, 
                                message, length, NULL);
	}
    free(name);
}

void file_recv(Tox *tox, uint32_t friendNum, uint32_t file_number, 
               uint32_t kind, uint64_t file_size, const uint8_t *filename, 
               size_t filename_length, void *user_data) {
	if (kind == TOX_FILE_KIND_AVATAR) {
		return;
	}

	tox_file_control(tox, friendNum, file_number, TOX_FILE_CONTROL_CANCEL, 
                     NULL);

	const char *msg = "i don't want your dumb file.";
	tox_friend_send_message(tox, friendNum, TOX_MESSAGE_TYPE_NORMAL, 
                            (uint8_t*)msg, strlen(msg), NULL);
}

void call(ToxAV *toxAV, uint32_t friendNum, bool audio_enabled, 
          bool video_enabled, void *user_data) {
	TOXAV_ERR_ANSWER err;
    toxav_answer(toxAV, friendNum, audio_enabled ? audio_bitrate : 0, 
                 video_enabled ? video_bitrate : 0, &err);

	if (err != TOXAV_ERR_ANSWER_OK) {
		printf("could not answer call, friend: %d, error: %d\n", 
               friendNum, err);
	}
}

void call_state(ToxAV *toxAV, uint32_t friendNum, uint32_t state, 
                void *user_data) {
	if (state & TOXAV_FRIEND_CALL_STATE_FINISHED) {
		printf("Call with friend %d finished\n", friendNum);
		return;
	} else if (state & TOXAV_FRIEND_CALL_STATE_ERROR) {
		printf("Call with friend %d errored\n", friendNum);
		return;
	}

	bool send_audio = (state & TOXAV_FRIEND_CALL_STATE_SENDING_A) 
                      && (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_A);
	bool send_video = state & TOXAV_FRIEND_CALL_STATE_SENDING_V 
                      && (state & TOXAV_FRIEND_CALL_STATE_ACCEPTING_V);
	toxav_bit_rate_set(toxAV, friendNum, send_audio ? audio_bitrate : 0,
                       send_video ? video_bitrate : 0, NULL);

	printf("Call state for friend %d changed to %d: audio: %d, video: %d\n", 
           friendNum, state, send_audio, send_video);
}

void audio_receive_frame(ToxAV *toxAV, uint32_t friendNum, 
                         const int16_t *pcm, size_t sample_count, 
                         uint8_t channels, uint32_t sampling_rate, 
                         void *user_data) {
	TOXAV_ERR_SEND_FRAME err;
	toxav_audio_send_frame(toxAV, friendNum, pcm, sample_count, channels, 
                           sampling_rate, &err);

	if (err != TOXAV_ERR_SEND_FRAME_OK) {
		printf("Could not send audio frame to friend: %d, error: %d\n", 
               friendNum, err);
	}
}

void video_receive_frame(ToxAV *toxAV, uint32_t friendNum, uint16_t width, 
                         uint16_t height, const uint8_t *y, const uint8_t *u, 
                         const uint8_t *v, int32_t ystride, int32_t ustride, 
                         int32_t vstride, void *user_data) {
	ystride = abs(ystride);
	ustride = abs(ustride);
	vstride = abs(vstride);

	if (ystride < width || ustride < width / 2 || vstride < width / 2) {
		printf("wtf\n");
		return;
	}

	uint8_t *y_dest = (uint8_t*)malloc(width * height);
	uint8_t *u_dest = (uint8_t*)malloc(width * height / 2);
	uint8_t *v_dest = (uint8_t*)malloc(width * height / 2);

	for (size_t h = 0; h < height; h++) {
		memcpy(&y_dest[h * width], &y[h * ystride], width);
	}

	for (size_t h = 0; h < height / 2; h++) {
		memcpy(&u_dest[h * width / 2], &u[h * ustride], width / 2);
		memcpy(&v_dest[h * width / 2], &v[h * vstride], width / 2);
	}

	TOXAV_ERR_SEND_FRAME err;
	toxav_video_send_frame(toxAV, friendNum, width, height, y_dest, 
                           u_dest, v_dest, &err);

	free(y_dest);
	free(u_dest);
	free(v_dest);

	if (err != TOXAV_ERR_SEND_FRAME_OK) {
		printf("Could not send video frame to friend: %d, error: %d\n", 
               friendNum, err);
	}
}

static void handle_signal(int sig) {
	signal_exit = true;
}

int main(int argc, char *argv[]) {
	signal(SIGINT, handle_signal);
	start_time = time(NULL);

	TOX_ERR_NEW err = TOX_ERR_NEW_OK;
	struct Tox_Options options;
	tox_options_default(&options);

    const char * homedir;

    if ((homedir = getenv("HOME")) == NULL) {
        homedir = getpwuid(getuid())->pw_dir;
    }   

    asprintf(&data_filename, "%s/.cache/tox_mrprickles", homedir);

	if (file_exists(data_filename)) {
		err = load_profile(&g_tox, &options);
        if (err == TOX_ERR_NEW_OK) {
			printf("Loaded data from %s\n", data_filename);
		} else {
			printf("Failed to load data from disk: error code%d\n", err);
			return -1;
		}
	} else {
		printf("Creating a new profile\n");

		g_tox = tox_new(&options, &err);

        if (err != TOX_ERR_NEW_OK) {
            printf("Error at tox_new, error: %d\n", err);
            return -1;
        }

        reset_info(g_tox);

        // set last_info_change to 0 to mean info hasn't been changed
        last_info_change = 0;
	}

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


	TOX_ERR_BOOTSTRAP err2;

    DHT_node nodes[] = {
        {"nodes.tox.chat",  33445, "788237D34978D1D5BD822F0A5BEBD2C53C64CC31CD3149350EE27D4D9A2F9B6B", {0}},
        {"144.76.60.215",   33445, "04119E835DF3E78BACF0F84235B300546AF8B936F035185E2A8E9E0A67C8924F", {0}},
        {"23.226.230.47",   33445, "A09162D68618E742FFBCA1C2C70385E6679604B2D80EA6E84AD0996A1AC8A074", {0}},
        {"178.21.112.187",  33445, "4B2C19E924972CB9B57732FB172F8A8604DE13EEDA2A6234E348983344B23057", {0}},
        {"195.154.119.113", 33445, "E398A69646B8CEACA9F0B84F553726C1C49270558C57DF5F3C368F05A7D71354", {0}},
        {"192.210.149.121", 33445, "F404ABAA1C99A9D37D61AB54898F56793E1DEF8BD46B1038B9D822E8460FAB67", {0}}
    };

    for (size_t i = 0; i < (sizeof(nodes)/sizeof(DHT_node)); i++) {
        printf("connecting to %s:%d...", nodes[i].ip, nodes[i].port);
        fflush(stdout);
        sodium_hex2bin(nodes[i].key_bin, sizeof(nodes[i].key_bin),
                       nodes[i].key_hex, sizeof(nodes[i].key_hex)-1, NULL, 
                       NULL, NULL);
        bool success = tox_bootstrap(g_tox, nodes[i].ip, nodes[i].port, 
                                     nodes[i].key_bin, &err2);
        if (success) {
            puts(" success!");
        } 
        if (err2 != TOX_ERR_BOOTSTRAP_OK) {
            printf("\nCould not bootstrap, error: %d\n", err2);
        }
    }

	if (err2 != TOX_ERR_BOOTSTRAP_OK) {
		printf("Could not bootstrap, error: %d\n", err2);
		return -1;
	}

	TOXAV_ERR_NEW err3;
	g_toxAV = toxav_new(g_tox, &err3);
	toxav_callback_call(g_toxAV, call, NULL);
	toxav_callback_call_state(g_toxAV, call_state, NULL);
	toxav_callback_audio_receive_frame(g_toxAV, audio_receive_frame, NULL);
	toxav_callback_video_receive_frame(g_toxAV, video_receive_frame, NULL);

	if (err3 != TOXAV_ERR_NEW_OK) {
		printf("Error at toxav_new: %d\n", err3);
		return -1;
	}

	pthread_t tox_thread, toxav_thread;
	pthread_create(&tox_thread, NULL, &run_tox, g_tox);
	pthread_create(&toxav_thread, NULL, &run_toxav, g_toxAV);

	while (!signal_exit) {
// 		usleep(500000L); // half a second -- why this length? 
        /* as i understand it, the call to sleep will be interrupted by sigint
           anyway. there is no need to waste cpu resources here...  */
		sleep(9999);
	}

	printf("Killing tox and saving profile\n");

	pthread_cancel(tox_thread);
	pthread_cancel(toxav_thread);

	save_profile(g_tox);
	toxav_kill(g_toxAV);
	tox_kill(g_tox);

	return 0;
}
