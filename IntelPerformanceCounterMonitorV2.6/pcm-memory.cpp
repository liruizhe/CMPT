/*
   Copyright (c) 2009-2012, Intel Corporation
   All rights reserved.

   Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

 * Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
 * Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
 * Neither the name of Intel Corporation nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
// written by Patrick Lu


/*!     \file pcm-memory.cpp
  \brief Example of using CPU counters: implements a performance counter monitoring utility for memory controller channels
  */
#define HACK_TO_REMOVE_DUPLICATE_ERROR
#include <iostream>
#ifdef _MSC_VER
#pragma warning(disable : 4996) // for sprintf
#include <windows.h>
#include "../PCM_Win/windriver.h"
#else
#include <unistd.h>
#include <signal.h>
#endif
#include <sys/time.h>
#include <math.h>
#include <iomanip>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <signal.h>
#include <assert.h>
#include "cpucounters.h"
#include "utils.h"
#include <time.h>
#include <sys/wait.h>
#include <setjmp.h>
#include "../libs/cmpt.h"

//Programmable iMC counter
#define READ 0
#define WRITE 1
#define PARTIAL 2

using namespace std;

static jmp_buf exit_point;

FILE * fw;
long counter = 0;
int cmpt_init_called = 0;

int flag_run;
PCM * pcm;
CoreCounterState * BeforeCoreStates;
CoreCounterState * AfterCoreStates;
ServerUncorePowerState * BeforeState;
ServerUncorePowerState * AfterState;
uint64 BeforeTime, AfterTime;
int is_cmpt_setting_enabled = 0;
int fd  = 0;
cmpt_data * cmpt_setting = NULL;
pid_t pid = 0;

void print_help(char * prog_name)
{
#ifdef _MSC_VER
	cout << " Usage: " << prog_name << " <delay>|\"external_program parameters\"|--help|--uninstallDriver|--installDriver <other options>" << endl;
#else
	cout << " Usage: " << prog_name << " <delay>|[external_program parameters]" << endl;
#endif
    cout << " Example: " << prog_name << " \"sleep 1\"" << endl;
    cout << " Example: " << prog_name << " 1" << endl;
    cout << endl;
}

void write_bandwidth(float *iMC_Rd_socket_chan, float *iMC_Wr_socket_chan, float *iMC_Rd_socket, float *iMC_Wr_socket, uint32 numSockets, uint32 num_imc_channels, uint64 *partial_write)
{
    float sysRead = 0.0, sysWrite = 0.0;
    uint32 skt = 0;

    while(skt < numSockets)
    {
		for(uint64 channel = 0; channel < num_imc_channels; ++channel)
		{
			if(iMC_Rd_socket_chan[skt*num_imc_channels+channel] < 0.0 && iMC_Wr_socket_chan[skt*num_imc_channels+channel] < 0.0) //If the channel read neg. value, the channel is not working; skip it.
			  continue;
			fprintf(fw, " %.2lf %.2lf", iMC_Rd_socket_chan[skt*num_imc_channels+channel], 
						iMC_Wr_socket_chan[skt*num_imc_channels+channel]);
		}
		fprintf(fw, " %.2lf", iMC_Rd_socket[skt]);
		fprintf(fw, " %.2lf", iMC_Wr_socket[skt]);
		fprintf(fw, " %lld", partial_write[skt]);
		fprintf(fw, " %.2lf", iMC_Rd_socket[skt] + iMC_Wr_socket[skt]);
		sysRead += iMC_Rd_socket[skt];
		sysWrite += iMC_Wr_socket[skt];
		skt += 1;
    }

	fprintf(fw, " %.2lf", sysRead);
	fprintf(fw, " %.2lf", sysWrite);
	fprintf(fw, " %.2lf", sysRead + sysWrite);
}

const uint32 max_sockets = 4;
const uint32 max_imc_channels = 8;

void calculate_bandwidth(PCM *m, const ServerUncorePowerState uncState1[], const ServerUncorePowerState uncState2[], uint64 elapsedTime)
{
    //const uint32 num_imc_channels = m->getMCChannelsPerSocket();
    float iMC_Rd_socket_chan[max_sockets][max_imc_channels];
    float iMC_Wr_socket_chan[max_sockets][max_imc_channels];
    float iMC_Rd_socket[max_sockets];
    float iMC_Wr_socket[max_sockets];
    uint64 partial_write[max_sockets];

    for(uint32 skt = 0; skt < m->getNumSockets(); ++skt)
    {
        iMC_Rd_socket[skt] = 0.0;
        iMC_Wr_socket[skt] = 0.0;
        partial_write[skt] = 0;

        for(uint32 channel = 0; channel < max_imc_channels; ++channel)
        {
            if(getMCCounter(channel,READ,uncState1[skt],uncState2[skt]) == 0.0 && getMCCounter(channel,WRITE,uncState1[skt],uncState2[skt]) == 0.0) //In case of JKT-EN, there are only three channels. Skip one and continue.
            {
                iMC_Rd_socket_chan[skt][channel] = -1.0;
                iMC_Wr_socket_chan[skt][channel] = -1.0;
                continue;
            }

			// in MB, 1 / 16384 = 64Byte / (1024 * 1024)
            iMC_Rd_socket_chan[skt][channel] = (float) (getMCCounter(channel,READ,uncState1[skt],uncState2[skt]) / 16384.0 / elapsedTime);
            iMC_Wr_socket_chan[skt][channel] = (float) (getMCCounter(channel,WRITE,uncState1[skt],uncState2[skt]) / 16384.0 / elapsedTime);

            iMC_Rd_socket[skt] += iMC_Rd_socket_chan[skt][channel];
            iMC_Wr_socket[skt] += iMC_Wr_socket_chan[skt][channel];

            partial_write[skt] += (uint64) (getMCCounter(channel,PARTIAL,uncState1[skt],uncState2[skt]) / elapsedTime);
        }
    }

    write_bandwidth(iMC_Rd_socket_chan[0], iMC_Wr_socket_chan[0], iMC_Rd_socket, iMC_Wr_socket, m->getNumSockets(), max_imc_channels, partial_write);
}

uint64 gettimestamp()
{
	struct timeval t;
	gettimeofday(&t, NULL);
	return (uint64)(t.tv_sec) * 1000000 + t.tv_usec;
}

void mycleanup(int s)
{
	fclose(fw);
    signal(s, SIG_IGN);
    PCM::getInstance()->cleanup();
    exit(0);
}

void timeup(int signal)
{
	AfterTime = gettimestamp();
	for(uint32 i=0; i<pcm->getNumSockets(); ++i)
	  AfterState[i] = pcm->getServerUncorePowerState(i);
	for(uint32 i=0; i<pcm->getNumCores(); ++i) {
		AfterCoreStates[i] = pcm->getCoreCounterState(i);
	}

	if (!cmpt_init_called && cmpt_setting->init_called) {
		cmpt_init_called = 1;
		fflush(fw);
		ftruncate(fileno(fw), (off_t)0);
		rewind(fw);
		printf("shm_init() on %ld\n", counter);
		counter = 0;
	}

	if (!is_cmpt_setting_enabled || cmpt_setting->enabled) {
		fprintf(fw, "%ld", counter);
		calculate_bandwidth(pcm,BeforeState,AfterState,AfterTime-BeforeTime);
		fprintf(fw, " [Inst]");

		for (uint32 i = 0; i < pcm->getNumCores(); ++i) {
			fprintf(fw, " %lld", getInstructionsRetired(BeforeCoreStates[i], AfterCoreStates[i]) * 1000000 / (AfterTime - BeforeTime));
		}
		fprintf(fw, " [FP]");
		for (uint32 i = 0; i < pcm->getNumCores(); ++i) {
			for (uint32 j = 0; j < 4; ++j) {
				fprintf(fw, " %lld", getNumberOfCustomEvents(j, BeforeCoreStates[i], AfterCoreStates[i]) * 1000000 / (AfterTime - BeforeTime));
			}
		}

		fprintf(fw, " [Other]");
		if (cmpt_setting->enabled_openmp_region)
		  fprintf(fw, " %d", cmpt_setting->in_openmp_region);
		else
		  fprintf(fw, " 1");
		fprintf(fw, "\n");
	}
	counter++;

	swap(BeforeTime, AfterTime);
	swap(BeforeState, AfterState);
	swap(BeforeCoreStates, AfterCoreStates);

	if (flag_run) {
		int status = 0;
		int ret = waitpid(pid, &status, WNOHANG);
		if (ret == 0)
		  return;
		if (ret < 0)
		  printf("Error!!!!\n");
		longjmp(exit_point, 1);
	}
}

int main(int argc, char * argv[])
{
#ifdef PCM_FORCE_SILENT
	null_stream nullStream1, nullStream2;
	std::cout.rdbuf(&nullStream1);
	std::cerr.rdbuf(&nullStream2);
#endif

	cout << endl;
	cout << " Intel(r) Performance Counter Monitor: Memory Bandwidth Monitoring Utility " << INTEL_PCM_VERSION << endl;
	cout << endl;
	cout << " Copyright (c) 2009-2013 Intel Corporation" << endl;
	cout << " This utility measures memory bandwidth per channel in real-time" << endl;
	cout << endl;
#ifdef _MSC_VER
	// Increase the priority a bit to improve context switching delays on Windows
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

	TCHAR driverPath[1032];
	GetCurrentDirectory(1024, driverPath);
	wcscat_s(driverPath, 1032, L"\\msr.sys");

	SetConsoleCtrlHandler((PHANDLER_ROUTINE)cleanup, TRUE);
#else
	signal(SIGPIPE, cleanup);
	signal(SIGINT, cleanup);
	signal(SIGKILL, cleanup);
	signal(SIGTERM, cleanup);
#endif

	int delay = 1;
	char * sysCmd = NULL;

	if (argc >= 2)
	{
		if (strcmp(argv[1], "--help") == 0 ||
					strcmp(argv[1], "-h") == 0 ||
                strcmp(argv[1], "/h") == 0)
        {
            print_help(argv[0]);
            return -1;
        }

#ifdef _MSC_VER
        if (strcmp(argv[1], "--uninstallDriver") == 0)
        {
            Driver tmpDrvObject;
            tmpDrvObject.uninstall();
            cout << "msr.sys driver has been uninstalled. You might need to reboot the system to make this effective." << endl;
            return 0;
        }
        if (strcmp(argv[1], "--installDriver") == 0)
        {
            Driver tmpDrvObject;
            if (!tmpDrvObject.start(driverPath))
            {
                cout << "Can not access CPU counters" << endl;
                cout << "You must have signed msr.sys driver in your current directory and have administrator rights to run this program" << endl;
                return -1;
            }
            return 0;
        }
#endif

        delay = atoi(argv[1]);
        if (delay <= 0)
        {
            sysCmd = argv[1];
        }
    }

#ifdef _MSC_VER
    // WARNING: This driver code (msr.sys) is only for testing purposes, not for production use
    Driver drv;
    // drv.stop();     // restart driver (usually not needed)
    if (!drv.start(driverPath))
    {
        cout << "Cannot access CPU counters" << endl;
        cout << "You must have signed msr.sys driver in your current directory and have administrator rights to run this program" << endl;
    }
#endif

	PCM::CustomCoreEventDescription eventDesc[4];
	eventDesc[0].event_number = 0x10; eventDesc[0].umask_value = 0x10;
	eventDesc[1].event_number = 0x10; eventDesc[1].umask_value = 0x20;
	eventDesc[2].event_number = 0x10; eventDesc[2].umask_value = 0x40;
	eventDesc[3].event_number = 0x10; eventDesc[3].umask_value = 0x80;
    pcm = PCM::getInstance();
    pcm->disableJKTWorkaround();
    PCM::ErrorCode status = pcm->program(PCM::CUSTOM_CORE_EVENTS, &eventDesc);
    switch (status)
    {
        case PCM::Success:
            break;
        case PCM::MSRAccessDenied:
            cout << "Access to Intel(r) Performance Counter Monitor has denied (no MSR or PCI CFG space access)." << endl;
            return -1;
        case PCM::PMUBusy:
            cout << "Access to Intel(r) Performance Counter Monitor has denied (Performance Monitoring Unit is occupied by other application). Try to stop the application that uses PMU." << endl;
            cout << "Alternatively you can try to reset PMU configuration at your own risk. Try to reset? (y/n)" << endl;
            char yn;
            std::cin >> yn;
            if ('y' == yn)
            {
                pcm->resetPMU();
                cout << "PMU configuration has been reset. Try to rerun the program again." << endl;
            }
            return -1;
        default:
            cout << "Access to Intel(r) Performance Counter Monitor has denied (Unknown error)." << endl;
            return -1;
    }
    
    cout << "\nDetected "<< pcm->getCPUBrandString() << " \"Intel(r) microarchitecture codename "<<pcm->getUArchCodename()<<"\""<<endl;
    if(!pcm->hasPCICFGUncore())
    {
        cout << "Jaketown, Ivytown or Haswell Server CPU is required for this tool!" << endl;
        if(pcm->memoryTrafficMetricsAvailable())
            cout << "For processor-level memory bandwidth statistics please use pcm.x" << endl;
        pcm->cleanup();
        return -1;
    }

    if(pcm->getNumSockets() > max_sockets)
    {
        cout << "Only systems with up to "<<max_sockets<<" sockets are supported! Program aborted" << endl;
        pcm->cleanup();
        return -1;
    }

	BeforeCoreStates = new CoreCounterState[pcm->getNumCores()];
	AfterCoreStates = new CoreCounterState[pcm->getNumCores()];

    BeforeState = new ServerUncorePowerState[pcm->getNumSockets()];
    AfterState = new ServerUncorePowerState[pcm->getNumSockets()];

	
    BeforeTime = 0, AfterTime = 0;

	char cmd[1024];
	uid_t uid;
	pid = 0;
	flag_run = 0;

	if (argc >= 3) {
		flag_run = 1;
		sprintf(cmd, "%s", argv[1]);
		sscanf(argv[2], "%d", &uid);
	}

	is_cmpt_setting_enabled = 0;
	fd  = 0;
	cmpt_setting = NULL;
	if ((fd = shm_open(SHARED_MEMORY_FILENAME, O_CREAT | O_RDWR, 0666)) != -1 && 
				ftruncate(fd, sizeof(cmpt_data)) == 0 &&
				(cmpt_setting = (cmpt_data *)mmap(NULL, sizeof(cmpt_data), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0)) != MAP_FAILED) {
	  is_cmpt_setting_enabled = 1;
	  cmpt_setting->enabled = 1;
	  cmpt_setting->init_called = 0;
	  cmpt_setting->enabled_openmp_region = 0;
	  cmpt_setting->in_openmp_region = 0;
	}
	else {
	  perror("shm_open");
	}

	fw = fopen("memory.csv", "w");
	if (flag_run) {
		pid = vfork();
		if (pid == 0) {
			setuid(uid);
			execl(cmd, cmd, NULL);
			return 0;
		} else if (pid < 0) {
			printf("Fork() error!\n");
			pcm->cleanup();
			return 0;
		}
	}
	BeforeTime = gettimestamp();
    for(uint32 i=0; i<pcm->getNumSockets(); ++i)
        BeforeState[i] = pcm->getServerUncorePowerState(i); 

	for (uint32 i=0; i<pcm->getNumCores(); ++i)
	  BeforeCoreStates[i] = pcm->getCoreCounterState(i);

	counter = 0;
	struct sigaction sa;
	struct itimerval timer;
	memset (&sa, 0, sizeof (sa));
	sa.sa_handler = &timeup;
	sigaction (SIGALRM, &sa, NULL);
	timer.it_value.tv_sec = 0;
	timer.it_value.tv_usec = 10000;
	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = 10000;
	setitimer(ITIMER_REAL, &timer, NULL);

	if (!setjmp(exit_point)) {
		while(1)
		{
			sleep(10);
		}
	}

	delete[] BeforeState;
	delete[] AfterState;

	shm_unlink(SHARED_MEMORY_FILENAME);
	pcm->cleanup();

	return 0;
}
