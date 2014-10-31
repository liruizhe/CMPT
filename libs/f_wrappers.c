#include "cmpt_lib.h"

#define cmpt_initf cmpt_initf_
#define cmpt_pausef cmpt_pausef_
#define cmpt_resumef cmpt_resumef_
#define cmpt_enter_openmpf cmpt_enter_openmpf_
#define cmpt_leave_openmpf cmpt_leave_openmpf_
#define cmpt_enable_openmp_regionf cmpt_enable_openmp_regionf_


int cmpt_initf(int is_pause_at_start)
{
	return cmpt_init(is_pause_at_start);
}

int cmpt_enable_openmp_regionf()
{
	return cmpt_enable_openmp_region();
}

int cmpt_pausef()
{
	return cmpt_pause();
}

int cmpt_resumef()
{
	return cmpt_resume();
}

int cmpt_enter_openmpf()
{
	return cmpt_enter_openmp();
}

int cmpt_leave_openmpf()
{
	return cmpt_leave_openmp();
}
