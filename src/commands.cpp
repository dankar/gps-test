#include "commands.h"
#include "gps.h"

typedef bool (*sms_handler_t)(gsm_t *, const char*);
extern gps_t gps;

bool send_position(gsm_t *gsm, const char *phone_no)
{
  char message[161];
  message[0] = '\0';
  gps_position_t position;
  bool is_valid = gps_get_position(&gps, &position);
  if (is_valid)
  {
    sprintf(message, "maps.google.com/?q=%.6f+%.6f\nHDOP: %.2f\nAge: %d\n", position.latitude, position.longitude, double(position.hdop)/ 100.0f, (millis() - position.timestamp)/1000);
  }
  else
  {
    strcat(message, "No GPS fix\n");
  }
  sprintf(message + strlen(message), "Bat: %d%% (%.3fV)\n", gsm->battery_percentage, gsm->battery_voltage);

  return gsm_send_sms(gsm, phone_no, message);
}

bool commands_handle_position(gsm_t *gsm, const char* phone_no)
{
  return send_position(gsm, phone_no);
}

bool commands_handle_list(gsm_t *gsm, const char* phone_no)
{
  char message[161];
  message[0] = '\0';

  for (uint8_t i = 0; i < GPS_NUM_HIGHSCORE; i++)
  {
    gps_position_t pos;
    if(gps_get_high_score(&gps, i, &pos))
    {
      sprintf(message + strlen(message), "%.6f,%.6f,%.2f,%d\n", pos.latitude, pos.longitude, double(pos.hdop) / 100.0f, (millis() - pos.timestamp) / 1000);
    }
  }

  return gsm_send_sms(gsm, phone_no, message);
}

char subscriber[30] = {0};

bool commands_handle_subscribe(gsm_t *gsm, const char* phone_no)
{
  strcpy(subscriber, phone_no);
  return gsm_send_sms(gsm, phone_no, "Subscribed!");
}

struct sms_command_t
{
  const char* command;
  sms_handler_t handler;
};

sms_command_t command_list[] = {
  { "STATUS", commands_handle_position },
  { "LIST", commands_handle_list },
  { "SUBSCRIBE", commands_handle_subscribe },
  { NULL, NULL }
};

bool commands_handle_sms_command(gsm_t *gsm, const char* phone_no, const char* content)
{
  uint8_t index = 0;
  String cmd_text = content;
  cmd_text.toUpperCase();
  for (;; index++)
  {
    if (command_list[index].command == NULL)
    {
      return false;
    }

    if (strcmp(cmd_text.c_str(), command_list[index].command) == 0)
    {
      return command_list[index].handler(gsm, phone_no);
    }
  }
}