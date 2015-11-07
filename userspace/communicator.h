#pragma once

class Communicator {
private:
	int event;
public:
	Communicator(int evnt);
	int throwEvent();
	int waitForEvent();
};	

#endif
