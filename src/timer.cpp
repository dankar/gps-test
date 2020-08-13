#include "timer.h"

void timer_reset(timer_t *timer)
{
	timer->deadline = millis() + timer->interval;
}

void timer_init(timer_t *timer, uint32_t interval)
{
	timer->interval = interval;
	timer_reset(timer);
}

bool timer_elapsed(timer_t *timer)
{
	if (millis() > timer->deadline)
	{
		timer_reset(timer);
		return true;
	}
	return false;
}