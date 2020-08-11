#ifndef _GSM_H_
#define _GSM_H_

#include <Arduino.h>
#include <SoftwareSerial.h>
#include "timer.h"

struct gsm_t;

typedef bool (*sms_callback_t)(gsm_t*, const char*, const char*);
typedef bool (*call_callback_t)(gsm_t*, const char*);

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

    bool disable_sms;
    bool monitor;
    bool debug;
};

bool gsm_init(gsm_t *gsm, SoftwareSerial *serial, sms_callback_t sms_callback, call_callback_t call_callback, bool disable_sms, bool monitor, bool debug);


void gsm_hangup(gsm_t *gsm);
bool gsm_first_setup(gsm_t *gsm);
bool gsm_send_sms(gsm_t *gsm, const char *phone_no, const char *message);
bool gsm_handle_call_id(gsm_t *gsm, char *caller_id);
bool gsm_handle_sms(gsm_t *gsm);

bool gsm_run(gsm_t *gsm, uint32_t time);


#endif