#ifndef _TIMER_H_
#define _TIMER_H_

#include <Arduino.h>

struct timer_t
{
  uint32_t deadline;
  uint32_t interval;
};

void timer_reset(timer_t *timer);
void timer_init(timer_t *timer, uint32_t interval);
bool timer_elapsed(timer_t *timer);

#endif