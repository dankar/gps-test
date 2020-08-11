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

  if(!gsm_init(&gsm, &gsm_uart, commands_handle_sms_command, send_position))
  {
    Serial.println("GSM failed");
    for(;;);
  }
}

void loop() {
  
  /*if (gsm->incoming_call)
  {
    char caller_id[20];
    if (gsm_handle_call_id(gsm, caller_id))
    {
      send_position(gsm, caller_id);
    }
    else
    {
      Serial.println("Timeout");
    }
    gsm_hangup(gsm);
  }*/

  gps_run(&gps, S(5));

  gps_position_t pos;

  gps_get_position(&gps, &pos);

  Serial.print("Lat: ");
  Serial.println(pos.latitude, 6);
  Serial.print("Lng: ");
  Serial.println(pos.longitude, 6);

  String message;

  for (uint8_t i = 0; i < 5; i++)
  {
    gps_position_t pos;
    if(gps_get_high_score(&gps, i, &pos))
    {
      message += String(pos.latitude, 6) + "," + String(pos.longitude, 6) + "," + String(float(pos.hdop) / 100.0f, 2) + "," + String((millis() - pos.timestamp) / 1000) + "\n";
    }
  }

  Serial.print(message);

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

  /*
      while (gsm->serial->available()) {
        Serial.write(gsm->serial->read());
    }
    while (Serial.available()) {
        gsm->serial->write(Serial.read());
    }*/
}