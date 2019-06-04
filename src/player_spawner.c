/*
 * player_spawner.c
 * 
 * Copyright 2019 Enrico Corradini <ecorradini@raspberryEnrico>
 *
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>

#include "libdl/dl_syscalls.h"

int main()
{
	//struct sched_attr attr;
	/*
	memset(&attr, 0, sizeof(struct sched_attr));
	attr.size = sizeof(struct sched_attr);
	attr.sched_policy = SCHED_DEADLINE;
	attr.sched_runtime  =  17000 * 1000ULL;
	attr.sched_period   = 100000 * 1000ULL;
	attr.sched_deadline = 100000 * 1000ULL;

	pid = sched_setattr(0, &attr, 0);
	if (pid < 0) {
		perror("Sched SetAttr");
		printf("ERRORE\n");
		return -1;
	}*/
  
	pid_t pid = fork();
	if (pid==0) {
		static char *argv[] = {"bin/videoplayer", "got.avi"};
		execv("bin/videoplayer",argv);
		exit(127);
	}
	else {
		waitpid(pid,0,0);
	}
	return 0;
}

