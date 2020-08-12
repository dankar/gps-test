#include "gps.h"
#include "timer.h"
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
    return uint16_t((millis()-pos->timestamp)/1000);
}

void gps_run(gps_t *gps, uint32_t time)
{
    timer_t timeout;
    timer_init(&timeout, time);

    gps->serial->listen();

    gps->has_valid_position = false;
    gps->current_position.hdop = 9999; // Reset hdop at beginning of cycle to store the best result

    while(!timer_elapsed(&timeout))
    {
        while (gps->serial->available())
        {
            gps_decoder.encode(gps->serial->read());
        }

        if(gps_decoder.location.isValid() && gps_decoder.hdop.isValid())
        {
            if(gps_decoder.hdop.value() < gps->current_position.hdop)
            {
                gps->current_position.latitude = gps_decoder.location.lat();
                gps->current_position.longitude = gps_decoder.location.lng();
                gps->current_position.hdop = gps_decoder.hdop.value();
                gps->current_position.timestamp = millis() - gps_decoder.location.age();
                gps->has_valid_position = true;
            }
        }        
    }

    // If we have a valid position, store it in the high scores
    if(gps->has_valid_position)
    {
        for (uint8_t i = 0; i < 5; i++)
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
                    for (uint8_t j = 4; j > i; j--)
                    {
                        gps->high_score[j] = gps->high_score[j - 1];
                    }

                    gps->high_score[i] = gps->current_position;
                    break;
                }
            }
        }
    }


}

bool gps_get_position(gps_t *gps, gps_position_t *out)
{
    if(!gps->has_valid_position)
    {
        return false;
    }
    memcpy(out, &gps->current_position, sizeof(gps_position_t));
    return true;
}
bool gps_get_high_score(gps_t *gps, int index, gps_position_t *out)
{
    if(index >= GPS_NUM_HIGHSCORE)
    {
        return false;
    }
    memcpy(out, &gps->high_score[index], sizeof(gps_position_t));
    return true;
}
