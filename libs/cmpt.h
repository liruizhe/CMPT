#ifndef _CMPT_H
#define _CMPT_H
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>

typedef struct cmpt_data_
{
	int init_called;
	int enabled;
} cmpt_data;

#define SHARED_MEMORY_FILENAME "/cmpt_lib_shm"

#endif //_CMPT_LIB_H
