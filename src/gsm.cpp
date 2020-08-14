#include "gsm.h"
#include "pins.h"
#include "util.h"
#include <EEPROM.h>

#define EEPROM_ENABLE_DATA_CONNECTION 0x0

struct data_type_t
{
	uint8_t index;
	char *out_data;
	uint8_t out_max_chars;
	char start_char;
	char stop_char;
};

struct tcp_packet_t
{
	double latitude;
	double longitude;
	double course; // deg
	double speed; // m/s
	uint16_t hdop; // 100ths
	uint16_t gps_age; // seconds
	uint8_t sats;
	double battery_voltage;
	uint8_t battery_percent;
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
			return false;
		}

		current_index = data[i].index;
		string_end_cause_t result = gsm_read_string(gsm, data[i].out_data, data[i].out_max_chars, data[i].start_char, data[i].stop_char, step_timeout);
		if (result == FAILURE)
		{
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
		return false;
	}

	if (!gsm_retrieve_data(gsm, data, num_entries, step_timeout))
	{
		return false;
	}

	if (!gsm_wait_for_response(gsm, "OK", step_timeout))
	{
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
			return false;
		}
	}

	if (!gsm_command(gsm, "AT+CFUN=1"))
	{
		return false;
	}

	delay(5000);

	if (!gsm_command(gsm, "AT+CMGD=1,4", "OK", 10000))
	{
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
		return false;
	}

	gsm->battery_voltage = atoi(voltage);
	gsm->battery_voltage /= 1000.0f;
	gsm->battery_percentage = atoi(percentage);

	return true;
}

void gsm_send_eod(gsm_t *gsm)
{
	gsm->serial->print('\x1A');
}

bool gsm_send_sms(gsm_t *gsm, const char *phone_no, const char *message)
{
	if (!gsm_command(gsm, "AT+CMGF=1"))
	{
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
			return false;
		}

		gsm_println(gsm, message);

		gsm_send_eod(gsm);

		if (!gsm_wait_for_response(gsm, "+CMGS:", SECONDS(30)))
		{
			return false;
		}
		if (!gsm_wait_for_response(gsm, "OK", SECONDS(10)))
		{
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
			return false;
		}

		if (gsm_read_string(gsm, text_scratch_pad, MAX_SMS_LENGTH, 0, '\x0d') == FAILURE)
		{
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

bool gsm_setup_gprs(gsm_t *gsm)
{
	if(!gsm_command(gsm, "AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""))
	{
		DEBUG_PRINTLN("Failed to set connection type");
		return false;
	}

	if(!gsm_command(gsm, "AT+SAPBR=3,1,\"APN\",\"4g.tele2.se\""))
	{
		DEBUG_PRINTLN("Failed to set APN");
		return false;
	}

	if(!gsm_command(gsm, "AT+SAPBR=1,1", "OK", SECONDS(30)))
	{
		DEBUG_PRINTLN("Failed to open bearer");
		return false;
	}

	return true;
}

bool gsm_check_gprs_status(gsm_t *gsm, bool enable)
{
	char status[2];
	data_type_t data[1] = { {1, status, 1, 0, 0 } };
	if(!gsm_command_and_retrieve_data(gsm, "AT+SAPBR=2,1", "+SAPBR", data, 1))
	{
		DEBUG_PRINTLN("Failed to get bearer status");
		return false;
	}

	if(atoi(status) == 1)
	{
		return true;
	}
	else
	{
		if(enable)
		{
			return gsm_setup_gprs(gsm);
		}
		else
		{
			return false;
		}
	}
}

bool gsm_init_tcp_connection(gsm_t *gsm)
{
	if(!gsm_command(gsm, "AT+CIPSTART=\"TCP\",\"www.danielkarling.se\",5195", "CONNECT OK", SECONDS(10)))
	{
		DEBUG_PRINTLN("Failed to open TCP");
		gsm->tcp_connection_active = false;
		return false;
	}
	gsm->tcp_connection_active = true;
	return true;
}

void gsm_tcp_shut(gsm_t *gsm)
{
	gsm_command(gsm, "AT+CIPSHUT", "SHUT OK", SECONDS(20));
	gsm->tcp_connection_active = false;
}

bool gsm_send_data(gsm_t *gsm, const char *data, uint16_t data_len)
{
	bool result = true;
	if(!gsm->tcp_connection_active)
	{
		if(!gsm_init_tcp_connection(gsm))
		{
			return false;
		}
	}

	text_scratch_pad[0] = '\0';
	sprintf(text_scratch_pad, "AT+CIPSEND=%d", data_len);
	gsm_println(gsm, text_scratch_pad);
	if (!gsm_wait_for_char(gsm, '>', SECONDS(3)))
	{
		result = false;
		goto cleanup;
	}

	for(uint16_t i = 0; i < data_len; i++)
	{
		gsm->serial->write(data[i]);
	}

	if (!gsm_wait_for_response(gsm, "OK", SECONDS(10)))
	{
		result = false;
	}

cleanup:
	if(result == false)
	{
		gsm_tcp_shut(gsm);
	}
	else
	{
		gsm->tcp_last_activity = millis();
	}
	return result;
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
	gsm->enable_data_connection = EEPROM.read(EEPROM_ENABLE_DATA_CONNECTION) == 1;
	timer_init(&gsm->battery_timer, SECONDS(5));
	timer_init(&gsm->sms_timer, SECONDS(5));
	timer_init(&gsm->check_gprs_timer, SECONDS(20));

	gsm->serial->begin(19200);

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
		delay(2500);
		gsm_flush(gsm);
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

bool gsm_run(gsm_t *gsm, gps_t *gps, uint32_t time)
{
	timer_t timeout;
	timer_init(&timeout, time);

	gsm->serial->listen();

	while (!timer_elapsed(&timeout))
	{
		if (gsm->debug)
		{
			for(;;)
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

			if(timer_elapsed(&gsm->check_gprs_timer))
			{
				if(gsm->enable_data_connection)
				{
					gsm->gprs_status = gsm_check_gprs_status(gsm, gsm->enable_data_connection);
					if(gsm->gprs_status)
					{
						gps_position_t pos;
						gps_get_position(gps, &pos);
						tcp_packet_t packet;
						packet.latitude = pos.latitude;
						packet.longitude = pos.longitude;
						packet.course = pos.course;
						packet.speed = pos.speed;
						packet.hdop = pos.hdop;
						packet.sats = pos.sats;
						packet.gps_age = gps_get_age_in_seconds(&pos);
						packet.battery_percent = gsm->battery_percentage;
						packet.battery_voltage = gsm->battery_voltage;
						gsm_send_data(gsm, (const char*)&packet, sizeof(tcp_packet_t));
					}

					if(millis() - gsm->tcp_last_activity > MINUTES(1))
					{
						resetFunc();
					}
				}
				else
				{
					if(gsm->tcp_connection_active)
					{
						gsm_tcp_shut(gsm);
						gsm_command(gsm, "AT+SAPBR=0,1");
					}
				}
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

void gsm_enable_data(gsm_t *gsm)
{
	gsm->enable_data_connection = true;
	gsm->tcp_last_activity = millis();
	EEPROM.write(EEPROM_ENABLE_DATA_CONNECTION, 1);
}
void gsm_disable_data(gsm_t *gsm)
{
	gsm->enable_data_connection = false;
	EEPROM.write(EEPROM_ENABLE_DATA_CONNECTION, 0);
}