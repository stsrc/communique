#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "events.h"

int main(void) {
	int rt;
	int cnt = 0;
	char event[2] = "a\0";
	event_set(event);	
	printf("proc2 entered\n");
	while(cnt < 4) {
		printf("proc2 sleeps on event.\n");
		rt = event_wait(event);
		printf("proc2 awaken.\n");
		event_check_error(rt, "event_wait");
		cnt++;
	}
	event_unset(event);	
	return 0;
}
