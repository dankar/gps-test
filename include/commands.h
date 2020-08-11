#ifndef _COMMANDS_H_
#define _COMMANDS_H_

#include <Arduino.h>
#include "gsm.h"

bool send_position(gsm_t *gsm, const char *phone_no);
bool commands_handle_sms_command(gsm_t *gsm, const char* phone_no, const char* content);

#endif