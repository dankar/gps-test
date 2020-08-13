#include "gsm.h"
#include "pins.h"
#include "util.h"

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
	if (gsm->monitor)
	{
		Serial.write(in);
	}
	return in;
}

inline void gsm_print(gsm_t *gsm, const char *out)
{
	if (gsm->monitor)
	{
		Serial.print(out);
	}
	gsm->serial->print(out);
}

inline void gsm_println(gsm_t *gsm, const char *out)
{
	if (gsm->monitor)
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

bool gsm_wait_for_char(gsm_t *gsm, char in, uint32_t to = DEFAULT_TIMEOUT)
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

bool gsm_wait_for_response(gsm_t *gsm, const char *response, uint32_t to = DEFAULT_TIMEOUT)
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

bool gsm_command_and_response(gsm_t *gsm, const char *command, const char *response, uint32_t timeout = DEFAULT_TIMEOUT)
{
	gsm_println(gsm, command);

	return gsm_wait_for_response(gsm, response, timeout);
}

bool gsm_command(gsm_t *gsm, const char *command, const char *wait_response = "OK", uint32_t to = DEFAULT_TIMEOUT)
{
	gsm_println(gsm, command);
	if (!gsm_wait_for_response(gsm, wait_response, to))
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
	while (gsm_check_for_call(gsm));
	DEBUG_PRINTLN("Hung up");
}

bool gsm_skip_commas(gsm_t *gsm, uint8_t commas, uint32_t to = DEFAULT_TIMEOUT)
{
	for (; commas;)
	{
		if (!gsm_wait_for_char(gsm, ',', to))
		{
			return false;
		}
		commas--;
	}

	return true;
}

enum string_end_cause_t
{
	END_CHAR,
	MAX_SIZE,
	FAILURE
};

string_end_cause_t gsm_read_string(gsm_t *gsm, char *out, uint8_t max_chars, char start_char, char end_char, uint32_t to = DEFAULT_TIMEOUT)
{
	uint8_t char_index = 0;
	timer_t timeout;
	timer_init(&timeout, to);

	if (start_char)
	{
		if (!gsm_wait_for_char(gsm, start_char, to))
		{
			DEBUG_PRINTLN("Failure waiting for start char");
			return FAILURE;
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
				return END_CHAR;
			}
			out[char_index++] = in;
			if (char_index == max_chars)
			{
				out[char_index] = '\0';
				return MAX_SIZE;
			}
		}
		if (timer_elapsed(&timeout))
		{
			DEBUG_PRINTLN("Timeout waiting for chars\n");
			return FAILURE;
		}
	}
}

bool gsm_retrieve_data(gsm_t *gsm, data_type_t *data, uint8_t num_entries, uint32_t step_timeout = DEFAULT_TIMEOUT)
{
	uint8_t current_index = 0;
	for (uint8_t i = 0; i < num_entries; i++)
	{
		if (!gsm_skip_commas(gsm, data[i].index - current_index, step_timeout))
		{
			DEBUG_PRINTLN("Error waiting for commas");
			return false;
		}

		current_index = data[i].index;
		string_end_cause_t result = gsm_read_string(gsm, data[i].out_data, data[i].out_max_chars, data[i].start_char, data[i].stop_char, step_timeout);
		if (result == FAILURE)
		{
			DEBUG_PRINTLN("Result is failure");
			return false;
		}
		else if (result == END_CHAR && data[i].stop_char == ',')
		{
			current_index++;
		}
	}

	return true;
}

bool gsm_command_and_retrieve_data(gsm_t *gsm, const char *command, const char *response, data_type_t *data, uint8_t num_entries, uint32_t step_timeout = DEFAULT_TIMEOUT)
{

	if (!gsm_command_and_response(gsm, command, response, step_timeout))
	{
		DEBUG_PRINTLN("Timeout waiting for response");
		return false;
	}

	if (!gsm_retrieve_data(gsm, data, num_entries, step_timeout))
	{
		DEBUG_PRINTLN("Timeout waiting for data");
		return false;
	}

	if (!gsm_wait_for_response(gsm, "OK", step_timeout))
	{
		DEBUG_PRINTLN("Error waiting for success");
		return false;
	}
	return true;
}

bool gsm_first_setup(gsm_t *gsm)
{
	if (!gsm_command(gsm, "AT"))
	{
		DEBUG_PRINTLN("Could not detect GSM");
		return false;
	}
	gsm_flush(gsm);

	if (!gsm->debug)
	{
		if (!gsm_command(gsm, "ATE0"))
		{
			DEBUG_PRINTLN("Could not disable echo");
			return false;
		}
	}

	if (!gsm_command(gsm, "AT+CFUN=1"))
	{
		DEBUG_PRINTLN("Could not set func");
		return false;
	}

	delay(5000);

	if (!gsm_command(gsm, "AT+CMGD=1,4", "OK", 10000))
	{
		DEBUG_PRINTLN("Could not remove old SMS");
		return false;
	}

	return true;
}

bool gsm_get_battery_status(gsm_t *gsm)
{
	char percentage[3];
	char voltage[5];

	data_type_t out_data[2] = {{1, percentage, 3, 0, ','}, {2, voltage, 4, 0, 0}};

	if (!gsm_command_and_retrieve_data(gsm, "AT+CBC", "+CBC", out_data, 2))
	{
		DEBUG_PRINTLN("Failed to get battery status");
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
		DEBUG_PRINTLN("Could not set text mode");
		return false;
	}

	DEBUG_PRINT("SMS To: ");
	DEBUG_PRINTLN(phone_no);
	DEBUG_PRINT("Content: \"");
	DEBUG_PRINT(message);
	DEBUG_PRINTLN("\"");


	gsm_flush(gsm);

	if (!gsm->disable_sms)
	{
		gsm_print(gsm, "AT+CMGS=\"");
		gsm_print(gsm, phone_no);
		gsm_println(gsm, "\"");
		if (!gsm_wait_for_char(gsm, '>', 3000))
		{
			DEBUG_PRINTLN("Could not send message");
			return false;
		}

		gsm_println(gsm, message);

		gsm->serial->print('\x1A');

		if (!gsm_wait_for_response(gsm, "+CMGS:", SECONDS(30)))
		{
			DEBUG_PRINTLN("Timeout sending SMS");
			return false;
		}
		if (!gsm_wait_for_response(gsm, "OK", SECONDS(10)))
		{
			DEBUG_PRINTLN("Error sending SMS");
			return false;
		}
	}
	return true;
}

bool gsm_handle_call_id(gsm_t *gsm, char *caller_id)
{
	data_type_t out_data[1] = {{5, caller_id, MAX_PHONE_NO_LENGTH, '\"', '\"'}};

	if (!gsm_command_and_retrieve_data(gsm, "AT+CLCC", "+CLCC", out_data, 1))
	{
		DEBUG_PRINTLN("Failed to get caller ID");
		return false;
	}

	return true;
}

bool gsm_handle_sms(gsm_t *gsm)
{
	if (!gsm_command(gsm, "AT+CMGF=1"))
	{
		return false;
	}

	if (!gsm_command_and_response(gsm, "AT+CMGL", "+CMGL"))
	{
		return false;
	}

	for (;;)
	{
		data_type_t out_data[1] = {{2, phone_scratch_pad, MAX_PHONE_NO_LENGTH, '\"', '\"'}};

		if (!gsm_retrieve_data(gsm, out_data, 1))
		{
			return false;
		}

		DEBUG_PRINT("SMS from: ");
		DEBUG_PRINTLN(phone_scratch_pad);

		if (!(gsm_wait_for_char(gsm, '\x0d') && gsm_wait_for_char(gsm, '\x0a')))
		{
			DEBUG_PRINTLN("Could not find text");
			return false;
		}

		if (gsm_read_string(gsm, text_scratch_pad, MAX_SMS_LENGTH, 0, '\x0d') == FAILURE)
		{
			DEBUG_PRINTLN("Could not get text");
			return false;
		}
		DEBUG_PRINT("Content: \"");
		DEBUG_PRINT(text_scratch_pad);
		DEBUG_PRINTLN("\"");

		gsm->sms_callback(gsm, phone_scratch_pad, text_scratch_pad);
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
	while (init_attempts < 3)
	{
		init_attempts++;
		DEBUG_PRINT("GSM init attempt ");
		DEBUG_PRINTLN(init_attempts);
		gsm_reset();
		delay(3000);
		if (gsm_first_setup(gsm))
		{
			init_attempts = 0;
			break;
		}
	}

	if (!init_attempts)
	{
		DEBUG_PRINTLN("GSM module initialized");

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

	while (!timer_elapsed(&timeout))
	{
		if (gsm->debug)
		{
			while (gsm->serial->available())
			{
				Serial.write(gsm->serial->read());
			}
			while (Serial.available())
			{
				gsm->serial->write(Serial.read());
			}
		}
		else
		{
			if (gsm_check_for_call(gsm))
			{
				bool success = false;
				if (gsm_handle_call_id(gsm, phone_scratch_pad))
				{
					success = true;
				}

				gsm_hangup(gsm);

				if (success)
				{
					gsm->call_callback(gsm, phone_scratch_pad);
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

void gsm_print_battery_status(gsm_t *gsm)
{
	Serial.print("Battery: ");
	Serial.print(gsm->battery_percentage);
	Serial.print("% (");
	Serial.print(gsm->battery_voltage);
	Serial.println("V)");
}