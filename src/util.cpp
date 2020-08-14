#include "util.h"

char phone_scratch_pad[MAX_PHONE_NO_LENGTH + 1];
char text_scratch_pad[MAX_SMS_LENGTH + 1];
void(* resetFunc) (void) = 0;