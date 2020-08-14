#include "gps.h"
#include "timer.h"
#include "util.h"
#include <TinyGPS++.h>

TinyGPSPlus gps_decoder;

bool gps_init(gps_t *gps, SoftwareSerial *serial)
{
	memset(gps, 0, sizeof(gps_t));
	gps->serial = serial;

	gps->serial->begin(9600);

	return true;
}

uint16_t gps_get_age_in_seconds(gps_position_t *pos)
{
	return uint16_t((millis() - pos->timestamp) / SECONDS(1));
}

void gps_high_score_move_forwards(gps_t *gps, uint8_t offset)
{
	for (uint8_t i = GPS_NUM_HIGHSCORE-1; i > offset; i--)
	{
		gps->high_score[i] = gps->high_score[i - 1];
	}
}

void gps_high_score_move_backwards(gps_t *gps, uint8_t offset)
{
	for (uint8_t i = offset; i < GPS_NUM_HIGHSCORE-1; i++)
	{
		gps->high_score[i] = gps->high_score[i + 1];
	}

	memset(&gps->high_score[GPS_NUM_HIGHSCORE-1], 0, sizeof(gps_position_t));
}

void gps_high_score_prune(gps_t *gps, uint32_t max_age)
{
	for(uint8_t i = 0; i < GPS_NUM_HIGHSCORE; i++)
	{
		if(gps->high_score[i].timestamp != 0)
		{
			uint32_t age = millis() - gps->high_score[i].timestamp;
			if(age > max_age)
			{
				gps_high_score_move_backwards(gps, i);
				i--;
			}
		}
	}
}

void gps_high_score_add_current(gps_t *gps)
{
	for (uint8_t i = 0; i < GPS_NUM_HIGHSCORE; i++)
	{
		if (gps->high_score[i].timestamp == 0)
		{
			gps->high_score[i] = gps->current_position;
			break;
		}
		else
		{
			if (gps->current_position.hdop < gps->high_score[i].hdop)
			{
				gps_high_score_move_forwards(gps, i);
				gps->high_score[i] = gps->current_position;
				break;
			}
		}
	}
}

void gps_run(gps_t *gps, uint32_t time)
{
	timer_t timeout;
	timer_init(&timeout, time);

	gps->serial->listen();

	gps_high_score_prune(gps, MINUTES(30));

	gps->has_valid_position = false;
	gps->current_position.hdop = 9999; // Reset hdop at beginning of cycle to store the best result

	while (!timer_elapsed(&timeout))
	{
		while (gps->serial->available())
		{
			gps_decoder.encode(gps->serial->read());
		}

		if (gps_decoder.location.isValid() && gps_decoder.hdop.isValid())
		{
			if (gps_decoder.hdop.value() < gps->current_position.hdop)
			{
				gps->current_position.latitude = gps_decoder.location.lat();
				gps->current_position.longitude = gps_decoder.location.lng();
				gps->current_position.hdop = gps_decoder.hdop.value();
				gps->current_position.timestamp = millis() - gps_decoder.location.age();
				gps->current_position.course = gps_decoder.course.deg();
				gps->current_position.speed = gps_decoder.speed.mps();
				gps->current_position.sats = gps_decoder.satellites.value();
				gps->has_valid_position = true;
				
			}
		}
	}

	// If we have a recent valid position, store it in the high scores
	if (gps->has_valid_position && gps_get_age_in_seconds(&gps->current_position) < 10)
	{
		gps_high_score_add_current(gps);
	}
}

bool gps_get_position(gps_t *gps, gps_position_t *out)
{
	memcpy(out, &gps->current_position, sizeof(gps_position_t));
	if (!gps->has_valid_position)
	{
		return false;
	}
	return true;
}
bool gps_get_high_score(gps_t *gps, int index, gps_position_t *out)
{
	if (index >= GPS_NUM_HIGHSCORE)
	{
		return false;
	}
	memcpy(out, &gps->high_score[index], sizeof(gps_position_t));
	return true;
}

void gps_print_position(gps_t *gps)
{
	gps_position_t pos;
	gps_get_position(gps, &pos);
	Serial.print("GPS: ");
	Serial.print(pos.latitude, 6);
	Serial.print(", ");
	Serial.print(pos.longitude, 6);
	Serial.print(", Sats: ");
	Serial.print(pos.sats);
	Serial.print(", HDOP:");
	Serial.println(float(pos.hdop)/100.0f, 2);
}

void gps_print_high_scores(gps_t *gps)
{
	for(uint8_t i = 0; i < GPS_NUM_HIGHSCORE; i++)
	{
		gps_position_t pos;
		gps_get_high_score(gps, i, &pos);
		Serial.print(pos.latitude, 6);
		Serial.print(",");
		Serial.print(pos.longitude, 6);
		Serial.print(",");
		Serial.print(float(pos.hdop)/100.0f, 2);
		Serial.print(",");
		Serial.println(gps_get_age_in_seconds(&pos));
	}
}
