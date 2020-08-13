#include "commands.h"
#include "gps.h"
#include "util.h"

typedef bool (*sms_handler_t)(gsm_t *, const char *);
extern gps_t gps;

bool send_position(gsm_t *gsm, const char *phone_no)
{
	text_scratch_pad[0] = '\0';
	gps_position_t position;
	bool is_valid = gps_get_position(&gps, &position);
	if (is_valid)
	{
		sprintf(text_scratch_pad, "maps.google.com/?q=%.6f+%.6f\nHDOP: %.2f\nAge: %d\n", position.latitude, position.longitude, double(position.hdop) / 100.0f, gps_get_age_in_seconds(&position));
	}
	else
	{
		strcat(text_scratch_pad, "No GPS fix\n");
	}
	sprintf(text_scratch_pad + strlen(text_scratch_pad), "Bat: %d%% (%.2fV)\n", gsm->battery_percentage, gsm->battery_voltage);

	return gsm_send_sms(gsm, phone_no, text_scratch_pad);
}

bool commands_handle_position(gsm_t *gsm, const char *phone_no)
{
	return send_position(gsm, phone_no);
}

bool commands_handle_list(gsm_t *gsm, const char *phone_no)
{
	text_scratch_pad[0] = '\0';

	for (uint8_t i = 0; i < GPS_NUM_HIGHSCORE; i++)
	{
		gps_position_t pos;
		if (gps_get_high_score(&gps, i, &pos))
		{
			sprintf(text_scratch_pad + strlen(text_scratch_pad), "%.6f,%.6f,%.2f,%d\n", pos.latitude, pos.longitude, double(pos.hdop) / 100.0f, gps_get_age_in_seconds(&pos));
		}
	}

	return gsm_send_sms(gsm, phone_no, text_scratch_pad);
}

#define MAX_SUBSCRIBERS 5
char subscriber[MAX_SUBSCRIBERS][MAX_PHONE_NO_LENGTH + 1] = {0};

bool send_subscription(gsm_t *gsm)
{
	for (uint8_t i = 0; i < MAX_SUBSCRIBERS; i++)
	{
		if (strlen(subscriber[i]))
		{
			send_position(gsm, subscriber[i]);
		}
	}

	return true;
}

bool add_subscriber(const char *phone_no)
{
	for (uint8_t i = 0; i < MAX_SUBSCRIBERS; i++)
	{
		if (!strlen(subscriber[i]))
		{
			strcpy(subscriber[i], phone_no);
			return true;
		}
	}
	return false;
}

bool commands_handle_subscribe(gsm_t *gsm, const char *phone_no)
{
	if (add_subscriber(phone_no))
	{
		gsm_send_sms(gsm, phone_no, "Subscribed. Send \"stop\" to end subscription.");
	}
	else
	{
		gsm_send_sms(gsm, phone_no, "Could not add you as subscriber.");
	}

	return true;
}

bool commands_handle_unsubscribe(gsm_t *gsm, const char *phone_no)
{
	for (uint8_t i = 0; i < MAX_SUBSCRIBERS; i++)
	{
		if (strcmp(phone_no, subscriber[i]) == 0)
		{
			subscriber[i][0] = '\0';
			gsm_send_sms(gsm, phone_no, "Unsubscribed");
			return true;
		}
	}
	return false;
}

struct sms_command_t
{
	const char *command;
	sms_handler_t handler;
};

sms_command_t command_list[] = {
	{"STATUS", commands_handle_position},
	{"LIST", commands_handle_list},
	{"SUBSCRIBE", commands_handle_subscribe},
	{"STOP", commands_handle_unsubscribe},
	{NULL, NULL}
};

void to_upper(char *str)
{
	while (*str)
	{
		*str = toupper(*str);
		str++;
	}
}

bool commands_handle_sms_command(gsm_t *gsm, const char *phone_no, const char *content)
{
	uint8_t index = 0;
	strcpy(text_scratch_pad, content);
	to_upper(text_scratch_pad);

	for (;; index++)
	{
		if (command_list[index].command == NULL)
		{
			return false;
		}

		if (strcmp(text_scratch_pad, command_list[index].command) == 0)
		{
			return command_list[index].handler(gsm, phone_no);
		}
	}
}