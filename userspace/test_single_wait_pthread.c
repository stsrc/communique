#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include "events.h"

/*
 * TEST SCENARIO:
 *2 thread - thr1 and thr2 - are talking (one sends, second waits)
 */

void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("thread 1 - sends event \"a\" 4 times\n");
	printf("thread 2 - captures event \"a\" 4 times\n");
	printf("on exit both threads unset event.\n\n");
	printf("To start press enter\n");
	getchar();
}

void thr1()
{
	char event[2] = "a\0";
	int cnt = 0;
	int rt = 0;
	printf("thread 1 - event_set(a)\n");
	int e0 = event_set(event);
	event_check_error_exit(e0, "event_set");
	sleep(1);
	while(cnt < 4) {
		sleep(1);
		printf("thread 1 - event_throw(a)\n");
		rt = event_throw(e0);
		if (rt == 1)
			continue;
		event_check_error(rt, "thr1 - event_throw");
		cnt++;
	}
	printf("thread 1 - event_unset(a)\n");
	rt = event_unset(e0);
	event_check_error_exit(rt, "thr1 - event_unset");
	sleep(1);
	printf("thread 1 - exits\n");
	return;
}

void *thr2()
{
	char event[2] = "a\0";
	int rt, eid;
	int cnt = 0;
	printf("thread 2 - event_set(a)\n");
	eid = event_set(event);
	event_check_error_exit(eid, "thr2 - event_set");
	while(cnt < 4) {
		printf("thread 2 - event_wait(a)\n");
		rt = event_wait(eid);
		event_check_error(rt, "thr2 - event_wait");
		cnt++;
	}
	printf("thread 2 - event_unset(a)\n");
	rt = event_unset(eid);
	event_check_error(rt, "thr2 - events_unset");
	printf("thread 2 - exits\n");
	return NULL;
}

int main(void)
{
	pthread_t thread_2;
	print_test_scenario();
        pthread_create(&thread_2, NULL, thr2, (void *)NULL);	
	thr1();
	return 0;
}
