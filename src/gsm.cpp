#include "gsm.h"
#include "pins.h"

struct data_type_t
{
  uint8_t index;
  char *out_data;
  uint8_t out_max_chars;
  char start_char;
  char stop_char;
};

inline char gsm_get_char(gsm_t *gsm)
{
  char in = gsm->serial->read();
  if(gsm->monitor)
  {
    Serial.write(in);
  }
  return in;
}

inline void gsm_print(gsm_t *gsm, const char* out)
{
  if(gsm->monitor)
  {
    Serial.print(out);
  }
  gsm->serial->print(out);
}

inline void gsm_println(gsm_t *gsm, const char* out)
{
  if(gsm->monitor)
  {
    Serial.println(out);
  }
  gsm->serial->println(out);
}

bool gsm_check_for_call(gsm_t *gsm)
{
  gsm->incoming_call = !digitalRead(GSM_RING);
  
  return gsm->incoming_call;
}

void gsm_flush(gsm_t *gsm)
{
  delay(500);
  while (gsm->serial->available()) 
  {
    gsm_get_char(gsm);
  }
}

bool gsm_wait_for_char(gsm_t *gsm, char in, uint32_t to = 1000)
{
  timer_t timeout;
  timer_init(&timeout, to);
  for (;;)
  {
    if (gsm->serial->available() && gsm_get_char(gsm) == in)
    {
      return true;
    }

    if (timer_elapsed(&timeout))
    {
      return false;
    }
  }
}

bool gsm_wait_for_response(gsm_t *gsm, const char *response, uint32_t to = 1000)
{
  uint32_t match_position = 0;
  timer_t timeout;
  timer_init(&timeout, to);
  for (;;)
  {
    if (gsm->serial->available())
    {
      if (gsm_get_char(gsm) == response[match_position])
      {
        match_position++;
      }
      else if (match_position > 0)
      {
        return false;
      }

      if (match_position == strlen(response))
      {
        return true;
      }
    }

    if (timer_elapsed(&timeout))
    {
      return false;
    }
  }
}

bool gsm_command_and_response(gsm_t *gsm, const char *command, const char *response, uint32_t timeout = 1000)
{
  gsm_println(gsm, command);

  return gsm_wait_for_response(gsm, response, timeout);
}

bool gsm_command(gsm_t *gsm, const char *command, char wait_char = 'K', uint32_t to = 1000)
{
  gsm_println(gsm, command);
  if (!gsm_wait_for_char(gsm, wait_char, to))
  {
    return false;
  }

  return true;
}

void gsm_hangup(gsm_t *gsm)
{
  gsm_flush(gsm);
  gsm_command(gsm, "ATH");
  gsm_flush(gsm);
  while(gsm_check_for_call(gsm));
  Serial.println("Hung up");
}

bool gsm_skip_commas(gsm_t *gsm, uint8_t commas, uint32_t to = 1000)
{
  timer_t timeout;
  timer_init(&timeout, to);
  for (;;)
  {
    if (gsm->serial->available())
    {
      char in = gsm_get_char(gsm);
      if (in == ',')
      {
        if (--commas == 0)
        {
          return true;
        }
      }
    }

    if (timer_elapsed(&timeout))
    {
      return false;
    }
  }
}

bool gsm_read_string(gsm_t *gsm, char *out, uint8_t max_chars, char start_char, char end_char, uint32_t to = 1000)
{
  uint8_t char_index = 0;
  timer_t timeout;
  timer_init(&timeout, to);

  if (start_char)
  {
    for (;;)
    {
      if (gsm->serial->available())
      {
        if (gsm_get_char(gsm) == start_char)
        {
          break;
        }
      }
      if (timer_elapsed(&timeout))
      {
        Serial.println("Timeout waiting for chars\n");
        return false;
      }
    }
  }

  for (;;)
  {
    if (gsm->serial->available())
    {
      char in = gsm_get_char(gsm);
      if (in == end_char)
      {
        out[char_index] = '\0';
        return true;
      }
      out[char_index++] = in;
      if (char_index == max_chars)
      {
        out[char_index] = '\0';
        return true;
      }
    }
    if (timer_elapsed(&timeout))
    {
      Serial.println("Timeout waiting for chars\n");
      return false;
    }
  }
}

bool gsm_retrieve_data(gsm_t *gsm, data_type_t *data, uint8_t num_entries, uint32_t step_timeout = 1000)
{
  uint8_t current_index = 0;
  for (uint8_t i = 0; i < num_entries; i++)
  {
    if (!gsm_skip_commas(gsm, data[i].index - current_index, step_timeout))
    {
      return false;
    }

    current_index = data[i].index;

    if (!gsm_read_string(gsm, data[i].out_data, data[i].out_max_chars, data[i].start_char, data[i].stop_char, step_timeout))
    {
      return false;
    }
  }


  return true;
}

bool gsm_command_and_retrieve_data(gsm_t *gsm, const char* command, const char *response, data_type_t *data, uint8_t num_entries, uint32_t step_timeout = 1000)
{

  if (!gsm_command_and_response(gsm, command, response, step_timeout))
  {
    Serial.println("Timeout waiting for response");
    return false;
  }


  if (!gsm_retrieve_data(gsm, data, num_entries, step_timeout))
  {
    Serial.println("Timeout waiting for data");
  }


  if (!gsm_wait_for_char(gsm, 'K', step_timeout))
  {
    Serial.println("Error waiting for success");
    return false;
  }
  return true;
}

bool gsm_first_setup(gsm_t *gsm)
{
  if (!gsm_command(gsm, "AT"))
  {
        Serial.println("Could not detect GSM");
        return false;
  }
  gsm_flush(gsm);

  if(!gsm->debug)
  {
    if(!gsm_command(gsm, "ATE0"))
    {
      Serial.println("Could not disable echo");
      return false;
    }
  }

  if (!gsm_command(gsm, "AT+CFUN=1"))
  {
    Serial.println("Could not set func");
    return false;
  }

  delay(5000);

  if(!gsm_command(gsm, "AT+CMGD=1,4", 'K', 10000))
  {
    Serial.println("Could not remove old SMS");
    return false;
  }

  return true;
}

bool gsm_get_battery_status(gsm_t *gsm)
{
  char percentage[3];
  char voltage[5];

  data_type_t out_data[2] = { { 1, percentage, 2, 0, 0 }, { 2, voltage, 4, 0, 0 } };

  if (!gsm_command_and_retrieve_data(gsm, "AT+CBC", "+CBC", out_data, 2))
  {
    Serial.println("Failed to get battery status");
    return false;
  }

  gsm->battery_voltage = atoi(voltage);
  gsm->battery_voltage /= 1000.0f;
  gsm->battery_percentage = atoi(percentage);
  
  return true;
}

bool gsm_send_sms(gsm_t *gsm, const char *phone_no, const char *message)
{
  if (!gsm_command(gsm, "AT+CMGF=1"))
  {
    Serial.println("Could not set text mode");
    return false;
  }

  Serial.print("SMS To: ");
  Serial.println(phone_no);
  Serial.print("Content: ");
  Serial.println(message);

  gsm_flush(gsm);

  if(!gsm->disable_sms)
  {
    gsm_print(gsm, "AT+CMGS=\"");
    gsm_print(gsm, phone_no);
    gsm_println(gsm, "\"");
    if (!gsm_wait_for_char(gsm, '>', 3000))
    {
      Serial.println("Could not send message");
      return false;
    }

    gsm_println(gsm, message);

    gsm->serial->print('\x1A');

    gsm_flush(gsm);
    delay(500);
    gsm_flush(gsm);
  }
  return true;
}

bool gsm_handle_call_id(gsm_t *gsm, char *caller_id)
{
    data_type_t out_data[1] = { { 5, caller_id, 19, '\"', '\"' }};

    if (!gsm_command_and_retrieve_data(gsm, "AT+CLCC", "+CLCC", out_data, 1))
    {
        Serial.println("Failed to get caller ID");
        return false;
    }

    return true;
}

bool gsm_handle_sms(gsm_t *gsm)
{
  if (!gsm_command(gsm, "AT+CMGF=1"))
  {
    Serial.println("Failed to set textmode");
        return false;
    }

    if (!gsm_command_and_response(gsm, "AT+CMGL", "+CMGL"))
    {
        return false;
    }

    char phone_no[30];
    char content[200];

    for (;;)
    {
        data_type_t out_data[1] = { { 2, phone_no, 29, '\"', '\"' }};

        if (!gsm_retrieve_data(gsm, out_data, 1))
        {
            return false;
        }

        Serial.println(phone_no);

        if (!(gsm_wait_for_char(gsm, '\x0d') && gsm_wait_for_char(gsm, '\x0a')))
        {
            Serial.println("Could not find text");
            return false;
        }

        if (!gsm_read_string(gsm, content, 199, 0, '\x0d'))
        {
            Serial.println("Could not get text");
            return false;
        }

        Serial.println(content);

        gsm->sms_callback(gsm, phone_no, content);
    }

    return true;
}

void gsm_reset()
{
  digitalWrite(GSM_ENABLE, LOW);
  delay(1100);
  digitalWrite(GSM_ENABLE, HIGH);
}

bool gsm_init(gsm_t *gsm, SoftwareSerial *serial, sms_callback_t sms_callback, call_callback_t call_callback, bool disable_sms, bool monitor, bool debug)
{
    uint8_t init_attempts = 0;

    memset(gsm, 0, sizeof(gsm_t));
    gsm->serial = serial;
    gsm->sms_callback = sms_callback;
    gsm->call_callback = call_callback;
    gsm->disable_sms = disable_sms;
    gsm->monitor = monitor;
    gsm->debug = debug;
    timer_init(&gsm->battery_timer, 5000);
    timer_init(&gsm->sms_timer, 5000);

    gsm->serial->begin(9600);

    pinMode(GSM_ENABLE, OUTPUT);
    pinMode(GSM_RING, INPUT);
    digitalWrite(GSM_RING, HIGH);
    gsm->serial->listen();
    while(init_attempts < 3)
    {
        init_attempts++;
        Serial.print("GSM init attempt ");
        Serial.println(init_attempts);
        gsm_reset();
        delay(2000);
        if(gsm_first_setup(gsm))
        {
            init_attempts = 0;
            break;
        }
    }

    if(!init_attempts)
    {
        Serial.println("GSM module initialized");
      
        return true;
    }
    else
    {
        return false;
    }
}

bool gsm_run(gsm_t *gsm, uint32_t time)
{
  timer_t timeout;
  timer_init(&timeout, time);

  gsm->serial->listen();

  while(!timer_elapsed(&timeout))
  {
    if(gsm->debug)
    {
      while (gsm->serial->available()) {
        Serial.write(gsm->serial->read());
      }
      while (Serial.available()) {
        gsm->serial->write(Serial.read());
      }
    }
    else
    {
      if(gsm_check_for_call(gsm))
      {
        char caller_id[30];
        bool success = false;
        if(gsm_handle_call_id(gsm, caller_id))
        {
          success = true;
        }

        gsm_hangup(gsm);

        if(success)
        {
          gsm->call_callback(gsm, caller_id);
        }
      }
      
      if (timer_elapsed(&gsm->battery_timer))
      {
          gsm_get_battery_status(gsm);
      }

      if (timer_elapsed(&gsm->sms_timer))
      {
          gsm_handle_sms(gsm);
      }

      gsm_flush(gsm);
    } 
  }
  return true;
}