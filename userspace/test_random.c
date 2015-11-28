#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <stdlib.h>
#include <time.h>

/*
 * TEST SCENARIO:
 *2 processes - proc1 and proc2 - talking (one sends, second waits)
 */

void proc1(char *event)
{
	int rt = 0;
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = rand()%1000001;
		nanosleep(&timespec, NULL);
		printf("proc1: event_throw.\n");
		rt = event_throw(event);
		if (rt == 1)
			continue;
		event_check_error_exit(rt, "proc1 - event_throw");
	}
	printf("proc1 - event unset\n");
	rt = event_unset(event);
	event_check_error_exit(rt, "proc1 - event_unset");
	exit(0);
}

void proc2(char *event)
{
	int rt;
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = rand()%1000001;
		nanosleep(&timespec, NULL);
		printf("proc2: event_wait.\n");
		rt = event_wait(event);
		event_check_error_exit(rt, "proc2 - event_wait");
	}
	exit(0);
}

void proc3(char *event)
{
	int rt;
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = rand()%1000001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_set.\n");
		rt = event_set(event);
		if (rt) {
			while(1) { perror("proc3: event_set"); }
		}
		event_check_error_exit(rt, "proc3: event_set\n");
		timespec.tv_nsec = rand()%1000001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_wait\n");
		rt = event_wait(event);
		event_check_error_exit(rt, "proc3: event_wait\n");
		timespec.tv_nsec = rand()%1000001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_throw\n");
		rt = event_throw(event);
		event_check_error_exit(rt, "proc3: event_throw\n");
				timespec.tv_nsec = rand()%1000001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_unset\n");
		rt = event_unset(event);
		if (rt) {
			while(1) {
				perror("proc3: event_unset");
			}
		}
		event_check_error_exit(rt, "proc3: event_unset\n");
	}
}

int main(void)
{
	char event[2] = "a\0";
	int rt;
	pid_t pid;
	rt = event_set(event);
	event_check_error(rt, "main: event_set");
	pid = fork();
	switch(pid) {
	case 0:
		proc2(event);		
		break;
	default:
		break;
	}
	pid = fork();
	switch(pid) {
	case 0:
		proc3(event);		
		break;
	default:
		break;
	}
	proc1(event);
	return 0;
}
