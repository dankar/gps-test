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
  Serial.println("Booting...");
  
  gps_init(&gps, &gps_uart);

  if(!gsm_init(&gsm, &gsm_uart, commands_handle_sms_command, send_position, true, false, false))
  {
    Serial.println("GSM failed");
    for(;;);
  }
}

void loop() {
  if(gsm.debug)
  {
    gsm_run(&gsm, S(5));
    return;
  }

  gps_run(&gps, S(5));

  gps_position_t pos;
  gps_get_position(&gps, &pos);
  Serial.print("GPS: ");
  Serial.print(pos.latitude, 6);
  Serial.print(", ");
  Serial.println(pos.longitude, 6);

  gsm_run(&gsm, S(5));

  Serial.print("Battery: ");
  Serial.print(gsm.battery_percentage);
  Serial.print("% (");
  Serial.print(gsm.battery_voltage);
  Serial.println("V)");
  

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