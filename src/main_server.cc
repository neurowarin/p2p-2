//std
#include <ctime>
#include <cstdlib>
#include <iostream>

#include "server.h"

int main(int argc, char * argv[])
{
	server Server;
	Server.start();

	while(true){
		sleep(1);
	}

	return 0;
}

