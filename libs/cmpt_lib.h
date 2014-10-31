#ifndef _CMPT_LIB_H
#define _CMPT_LIB_H

extern int cmpt_init(int is_pause_at_start);
extern int cmpt_enable_openmp_region();
extern int cmpt_pause();
extern int cmpt_resume();
extern int cmpt_enter_openmp();
extern int cmpt_leave_openmp();

#endif //_CMPT_LIB_H
