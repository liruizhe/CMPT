#include "cmpt_lib.h"

#define cmpt_initf cmpt_initf_
#define cmpt_pausef cmpt_pausef_
#define cmpt_resumef cmpt_resumef_

int cmpt_initf(int is_pause_at_start)
{
	return cmpt_init(is_pause_at_start);
}

int cmpt_pausef()
{
	return cmpt_pause();
}

int cmpt_resumef()
{
	return cmpt_resume();
}
