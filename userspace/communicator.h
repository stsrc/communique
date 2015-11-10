#pragma once

#define SETEVENT (1 << 30) | (sizeof(int *) << 16) | (0x8A << 8) | 0x01
#define WAITFOREVENT (1 << 30) | (sizeof(int *) << 16) | (0x8A << 8) | 0x02
#define	THROWEVENT (1 << 30) | (sizeof(int *) << 16) | (0x8A << 8) | 0x03

class Communicator {
private:
	int event;
public:
	Communicator(int evnt);
	int throwEvent();
	int waitForEvent();
};	

#endif
