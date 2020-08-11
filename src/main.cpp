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

int incoming_call = 0;

timer_t gsm_subscriber_timer;

gps_t gps;
gsm_t gsm;

void setup() {
  // put your setup code here, to run once:
  
  Serial.begin(115200);
  Serial.println("REBOOT!!!");
  
  gps_init(&gps, &gps_uart);

  if(!gsm_init(&gsm, &gsm_uart, commands_handle_sms_command, send_position, true, false, false))
  {
    Serial.println("GSM failed");
    for(;;);
  }
}

extern char scratch_pad[161];

void loop() {
  gps_run(&gps, S(5));

  gps_position_t pos;

  gps_get_position(&gps, &pos);

  Serial.println("GPS pos:");
  Serial.print("Lat: ");
  Serial.println(pos.latitude, 6);
  Serial.print("Lng: ");
  Serial.println(pos.longitude, 6);

  scratch_pad[0] = '\0';

  Serial.println("GPS list:");
  for (uint8_t i = 0; i < GPS_NUM_HIGHSCORE; i++)
  {
    gps_position_t pos;
    if(gps_get_high_score(&gps, i, &pos))
    {
      sprintf(scratch_pad + strlen(scratch_pad), "%.6f,%.6f,%.2f,%d\n", pos.latitude, pos.longitude, double(pos.hdop) / 100.0f, (millis() - pos.timestamp) / 1000);
    }
  }

  Serial.print(scratch_pad);

  gsm_run(&gsm, S(5));

  /*if (timer_elapsed(&gsm_subscriber_timer))
  {

    if (strlen(subscriber))
    {
      Serial.println("Sending subscription");
      gsm_send_position(subscriber);
    }
    else
    {
      Serial.println("No subscriber");
    }
  }*/
}