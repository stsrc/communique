#include <stdio.h>
#include <unistd.h>
#include "events.h"
#include <errno.h>
#include <pthread.h>

/*
 * TEST SCENARIO:
 * first thread throws one of the four event,
 * second thread in group_wait
 */

void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("Thread 1 throws one of four events\n");
	printf("Thread 2 waits on group_wait - waits for one of four events\n");
	printf("Thread 2 knows on which event it was awaken\n\n");
}

void *thr2()
{
	int cnt = 0;
	int rt, e0, e1, e2, e3;
	char eventa[4] = "ab\0";
	char eventb[4] = "bcd\0";
	char eventc[4] = "cde\0";
	char eventd[4] = "d\0";
	char *events[] = {eventa, eventb, eventc, eventd};
	printf("thread 2: event_set\n");
	e0 = event_set(eventa);
	event_check_error_exit(e0, "thread 2: event_set");
	e1 = event_set(eventb); 
	event_check_error_exit(e1, "thread 2: event_set");
	e2 = event_set(eventc);
	event_check_error_exit(e2, "thread 2: event_set");
	e3 = event_set(eventd);
	event_check_error_exit(e3, "thread 2: event_set");
	while(cnt < 4) {
		printf("thread 2: event_wait\n");
		rt = event_wait_group(events, 4);
		if (rt > 0)
			printf("thread 2: event %s caught\n", events[rt-1]);
		event_check_error_exit(rt, "thread 2: event_wait FAILS!");
		cnt++;
	}
	printf("thread 2: Unsetting events\n");
	rt = event_unset(e0);
	event_check_error(rt, "thread 2: event_unset event0");
	rt = event_unset(e1);
	event_check_error(rt, "thread 2: event_unset event1");
	rt = event_unset(e2);
	event_check_error(rt, "thread 2: event_unset event2");
	rt = event_unset(e3);
	event_check_error(rt, "thread 2: event_unset event3");
	printf("thread 2: returning\n");
	return NULL;
}

int main(void)
{
	int cnt = 0;
	char eventa[4] = "ab\0";
	char eventb[4] = "bcd\0";
	char eventc[4] = "cde\0";
	char eventd[4] = "d\0";
	char *events[] = {eventa, eventb, eventc, eventd};
	int rt, e0, e1, e2, e3;
	int tab[4];
	pthread_t thread_2;
	rt = pthread_create(&thread_2, NULL, thr2, (void *)NULL);
	printf("thread 1: event_set\n");
	e0 = event_set(eventa);
	event_check_error_exit(e0, "thread 1: event_set");
	e1 = event_set(eventb);
	event_check_error_exit(e1, "thread 1: event_set");
	e2 = event_set(eventc);
	event_check_error_exit(e2, "thread 1: event_set");
	e3 = event_set(eventd);
	event_check_error_exit(e3, "thread 1: event_set");
	tab[0] = e0;
	tab[1] = e1;
	tab[2] = e2;
	tab[3] = e3;
	sleep(1);
	while(cnt < 4) {
		sleep(1);
		printf("thread 1: event_throw, event %s\n", events[cnt]);
		rt = event_throw(tab[cnt]);
		event_check_error_exit(rt, "thread 1: event_throw");
		cnt++;
	}
	printf("thread1: Unsetting events\n");
	rt = event_unset(e0);
	event_check_error(rt, "thread 1: event_unset event0");
	rt = event_unset(e1);
	event_check_error(rt, "thread 1: event_unset event1");
	rt = event_unset(e2);
	event_check_error(rt, "thread 1: event_unset event2");
	rt = event_unset(e3);
	event_check_error(rt, "thread 1: event_unset event3");
	sleep(1);
	printf("thread 1: returning\n");
	return 0;
}
