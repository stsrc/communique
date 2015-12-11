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
	int eid = event_set(event);
	event_check_error(eid, "main: event_set");
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = rand()%2000001;
		nanosleep(&timespec, NULL);
		printf("proc1: event_throw.\n");
		rt = event_throw(eid);
		if (rt == 1)
			continue;
		event_check_error_exit(rt, "proc1 - event_throw");
	}
	printf("proc1 - event unset\n");
	rt = event_unset(eid);
	event_check_error_exit(rt, "proc1 - event_unset");
	exit(0);
}

void proc2(char *event)
{
	int rt;
	int eid = event_set(event);
	event_check_error(eid, "proc2 - event_set");
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = rand()%1000001;
		nanosleep(&timespec, NULL);
		printf("proc2: event_wait.\n");	
		rt = event_wait(eid);
		event_check_error_exit(rt, "proc2 - event_wait");
	}
	exit(0);
}

void proc3(char *event)
{
	int rt, eid;
	struct timespec timespec;
	clock_gettime(CLOCK_REALTIME, &timespec);
	srand((int)timespec.tv_nsec);
	timespec.tv_sec = 0;
	while(1) {
		timespec.tv_nsec = rand()%50001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_set.\n");
		eid = event_set(event);
		if (eid < 0) {
			while(1) { perror("proc3: event_set"); }
		}
		event_check_error_exit(rt, "proc3: event_set\n");
		timespec.tv_nsec = rand()%50001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_wait\n");
		rt = event_wait(eid);
		event_check_error_exit(rt, "proc3: event_wait\n");
		timespec.tv_nsec = rand()%50001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_throw\n");
		rt = event_throw(eid);
		event_check_error_exit(rt, "proc3: event_throw\n");
		timespec.tv_nsec = rand()%50001;
		nanosleep(&timespec, NULL);
		printf("proc3: event_unset\n");
		rt = event_unset(eid);
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
	pid_t pid;
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
