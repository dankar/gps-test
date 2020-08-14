#ifndef _UTIL_H_
#define _UTIL_H_

#define MAX_SMS_LENGTH 160
#define MAX_PHONE_NO_LENGTH 14
#define SECONDS(x) (x * 1000UL) // seconds
#define MINUTES(x) SECONDS(60 * x)
#define DEFAULT_TIMEOUT SECONDS(1)

extern char phone_scratch_pad[MAX_PHONE_NO_LENGTH + 1];
extern char text_scratch_pad[MAX_SMS_LENGTH + 1];

#if 0
#define DEBUG_PRINTLN(x) Serial.println(x)
#define DEBUG_PRINT(x) Serial.print(x)
#else
#define DEBUG_PRINTLN(x) (void)x
#define DEBUG_PRINT(x) (void)x
#endif

#endif