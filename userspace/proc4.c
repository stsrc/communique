#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include "events.h"

int main(void) {
	int rt;
	char *event[] = {"RAZ\0", "DWA\0", "TRZY\0", "CZTERRY\0"};
	rt = event_wait_group(event, 4);
	event_check_error(rt, "event_wait_in_groyp");
	return 0;
}
