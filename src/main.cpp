#include <SoftwareSerial.h>
#include "gps.h"
#include "timer.h"
#include "pins.h"
#include "gsm.h"
#include "commands.h"
#include "util.h"

#define SEND_SMS 1
#define DEBUG 0
#ifndef DEBUG
#define DEBUG 0
#endif

SoftwareSerial gps_uart(GPS_RX, GPS_TX);
SoftwareSerial gsm_uart(GSM_RX, GSM_TX);

timer_t gsm_subscriber_timer;

gps_t gps;
gsm_t gsm;

void(* resetFunc) (void) = 0;

void setup()
{
	Serial.begin(115200);
	Serial.println("Booting...");

	gps_init(&gps, &gps_uart);

	if (!gsm_init(&gsm, &gsm_uart, commands_handle_sms_command, send_position))
	{
		resetFunc();
	}

	timer_init(&gsm_subscriber_timer, MINUTES(10));
}

void loop()
{
	gps_run(&gps, SECONDS(2));
	gps_print_position(&gps);
	gps_print_high_scores(&gps);

	gsm_run(&gsm, SECONDS(5));
	gsm_print_battery_status(&gsm);

	if (timer_elapsed(&gsm_subscriber_timer))
	{
		send_subscription(&gsm);
	}
}