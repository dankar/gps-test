#include "commands.h"
#include "gps.h"

typedef bool (*sms_handler_t)(gsm_t *, const char*);
extern gps_t gps;

bool send_position(gsm_t *gsm, const char *phone_no)
{
  String message = "";
  gps_position_t position;
  bool is_valid = gps_get_position(&gps, &position);
  if (is_valid)
  {
    message += "maps.google.com/?q=" + String(position.latitude, 6) + "+" + String(position.latitude, 6) + "\n";
    message += "HDOP: " + String((float(position.hdop) / 100.0f)) + "\n";
    message += "Age: " + String((millis() - position.timestamp) / 1000) + "s\n";
  }
  else
  {
    message += "No GPS fix\n";
  }
  message += "Bat: " + String(gsm->battery_percentage) + "% (" + String(gsm->battery_voltage) + "V)\n";

  return gsm_send_sms(gsm, phone_no, message.c_str());
}

bool commands_handle_position(gsm_t *gsm, const char* phone_no)
{
  return send_position(gsm, phone_no);
}

bool commands_handle_list(gsm_t *gsm, const char* phone_no)
{
  String message;

  for (uint8_t i = 0; i < 5; i++)
  {
    gps_position_t pos;
    if(gps_get_high_score(&gps, i, &pos))
    {
      message += String(pos.latitude, 6) + "," + String(pos.longitude, 6) + "," + String(float(pos.hdop) / 100.0f, 2) + "," + String((millis() - pos.timestamp) / 1000) + "\n";
    }
  }

  return gsm_send_sms(gsm, phone_no, message.c_str());
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