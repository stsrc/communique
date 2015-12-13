#include <stdio.h>
#include <unistd.h>
#include "events.h"

/*
 * Scenariusz testu:
 * glob_name_size: 1 proces tworzy zdarzenie o zbyt dlugiej nazwie.
 * Nie zostalo to zaimplementowane.
 *
 *    Parametry modulu do testow:
 *	glob_name_size = 5;
 *	glob_event_cnt_max = 5;
 *	glob_compl_cnt_max = 5;
 *	glob_proc = 5;
 */

void print_test_scenario()
{
	printf("\nTest scenario:\n\n");
	printf("Creating event with to long name.\n"
	       "Module parameters:\nglob_name_size = 5;\n"
	       "glob_event_cnt_max = 5;\nglob_compl_cnt_max = 5;\n"
	       "glob_proc = 5;\n\n");
	printf("Press enter to start.\n");
	getchar();	
}

int main(void)
{
	int e0;
	char *event = "-------------------------------\0";
	print_test_scenario();
	printf("proc1: event_set. Should fail.\n");
	e0 = event_set(event);
	event_check_error_exit(e0, "proc1: event_set. Positive fail");
	printf("proc1: event_set have not failed!\n");
	printf("proc1: event_unset.\n");
	event_unset(e0);
	printf("proc1: exits.\n");
	return 0;
}
