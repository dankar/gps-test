#ifndef _GPS_H_
#define _GPS_H_

#include <Arduino.h>
#include <SoftwareSerial.h>

#define GPS_NUM_HIGHSCORE 5

struct gps_position_t
{
  uint32_t timestamp;
  double latitude;
  double longitude;
  uint16_t hdop;
};

struct gps_t
{
    gps_position_t current_position;
    gps_position_t high_score[GPS_NUM_HIGHSCORE];
    SoftwareSerial *serial;
    bool has_valid_position;
};

bool gps_init(gps_t *gps, SoftwareSerial *serial);
void gps_run(gps_t *gps, uint32_t time);

bool gps_get_position(gps_t *gps, gps_position_t *out);
bool gps_get_high_score(gps_t *gps, int index, gps_position_t *out);

#endif