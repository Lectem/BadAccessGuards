#include <thread>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#endif
#include "BadAccessGuards.h"

BadAccessGuardShadow someglobal;

int main()
{
	{
		printf("Testing read during write on the same thread, output:\n");

		BadAccessGuardWrite writeg{ someglobal };
		BadAccessGuardRead readg{ someglobal };
	}


	{
		printf("\n\nTesting read during write on different threads, output:\n");

		using namespace std::chrono_literals;
		std::thread otherthread([] {
#ifdef _WIN32
			SetThreadDescription(GetCurrentThread(), L"ØUnsafe WriterØ");
#endif
			BadAccessGuardWrite writeg{ someglobal };
			std::this_thread::sleep_for(3s);
			});
		std::this_thread::sleep_for(1s);
		// Assuming the other thread has been scheduled. No sync on purpose since we want to see if our code could detect the race conditon.
		BadAccessGuardRead readg{ someglobal };
		otherthread.join();
	}


	{
		printf("\n\nTesting write during write on different threads, output:\n");

		using namespace std::chrono_literals;
		std::thread otherthread([] {
#ifdef _WIN32
			SetThreadDescription(GetCurrentThread(), L"ØUnsafe WriterØ");
#endif
			BadAccessGuardWrite writeg{ someglobal };
			std::this_thread::sleep_for(3s);
			});
		std::this_thread::sleep_for(1s);
		// Assuming the other thread has been scheduled. No sync on purpose since we want to see if our code could detect the race conditon.
		BadAccessGuardWrite readg{ someglobal };
		otherthread.join();
	}


	return 0;
}
