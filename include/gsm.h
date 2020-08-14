#ifndef _GSM_H_
#define _GSM_H_

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "timer.h"
#include "gps.h"

struct gsm_t;

typedef bool (*sms_callback_t)(gsm_t *, const char *, const char *);
typedef bool (*call_callback_t)(gsm_t *, const char *);

struct gsm_t
{
	double battery_voltage;
	uint8_t battery_percentage;
	SoftwareSerial *serial;
	bool incoming_call;
	sms_callback_t sms_callback;
	call_callback_t call_callback;

	timer_t battery_timer;
	timer_t sms_timer;
	timer_t check_gprs_timer;

	bool disable_sms;
	bool monitor;
	bool debug;

	bool enable_data_connection;
	bool gprs_status;

	bool tcp_connection_active;
	uint32_t tcp_last_activity;
};

bool gsm_init(gsm_t *gsm, SoftwareSerial *serial, sms_callback_t sms_callback, call_callback_t call_callback, bool disable_sms = false, bool monitor = false, bool debug = false);

void gsm_hangup(gsm_t *gsm);
bool gsm_first_setup(gsm_t *gsm);
bool gsm_send_sms(gsm_t *gsm, const char *phone_no, const char *message);
bool gsm_handle_call_id(gsm_t *gsm, char *caller_id);
bool gsm_handle_sms(gsm_t *gsm);

bool gsm_run(gsm_t *gsm, gps_t *gps, uint32_t time);

void gsm_print_battery_status(gsm_t *gsm);

void gsm_enable_data(gsm_t *gsm);
void gsm_disable_data(gsm_t *gsm);

#endif