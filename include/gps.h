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
	double course;
	double speed;
	uint8_t sats;
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

uint16_t gps_get_age_in_seconds(gps_position_t *pos);

bool gps_get_position(gps_t *gps, gps_position_t *out);
bool gps_get_high_score(gps_t *gps, int index, gps_position_t *out);

void gps_print_position(gps_t *gps);
void gps_print_high_scores(gps_t *gps);

#endif