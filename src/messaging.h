#pragma once

#include <tox/tox.h>

void reply_friend_message(Tox *tox, uint32_t friend_num, char *dest_msg, size_t length);
