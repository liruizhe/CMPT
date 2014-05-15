#include <stdio.h>
#include "cmpt_lib.h"
#include "cmpt.h"

static int is_shm_exists = 0;
static cmpt_data * data = NULL;

int cmpt_init(int is_pause_at_start)
{
	int fd = 0;
	struct stat stat;
	if ((fd = shm_open(SHARED_MEMORY_FILENAME, O_RDWR, 0)) != -1 && fstat(fd, &stat) != -1) {
		if (stat.st_size != sizeof(cmpt_data))
		  return -1;
		data = mmap(NULL, stat.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (data == MAP_FAILED)
		  return -1;
		is_shm_exists = 1;
		data->enabled = ! is_pause_at_start;
		data->init_called = 1;
	}
	else
	  perror("cmpt_init");
	return 0;
}

int cmpt_pause()
{
	if (is_shm_exists)
	  data->enabled = 0;
	return 0;
}

int cmpt_resume()
{
	if (is_shm_exists)
	  data->enabled = 1;
	return 0;
}

int cmpt_enter_openmp()
{
	if (is_shm_exists)
	  data->in_openmp_area = 1;
	return 0;
}

int cmpt_leave_openmp()
{
	if (is_shm_exists)
	  data->in_openmp_area = 0;
	return 0;
}
