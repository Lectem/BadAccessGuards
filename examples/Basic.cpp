#include <thread>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <new>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <BadAccessGuards.h>

int main(int argv, char** argc)
{
	bool allowBreak = false;
	if (argv >= 2 && !strcmp(argc[1], "--break"))
	{
		allowBreak = true;
	}
	else
	{
		printf("Run with `--break` to make this example break in the debugger.\n\n");
	}
#if BAD_ACCESS_GUARDS_ENABLE
	// Don't break for this sample as the race condition is controlled and won't trigger a crash, so we'll just be printing!
	

	BadAccessGuardSetConfig({
			allowBreak, // allowBreak
			false, // breakASAP
		});
#else
#error "Can't really test the guards if we don't enable them can we ?"
#endif


	// To test memory corruption/use after free, we use placement to control the lifetime of the object, and its memory
	// In the real world, freeing memory won't always (and most often don't) release the memory pages, so it is still accessible
	// Here we keep the backing memory (shadowAlignedStorage) alive for the duration of the test, even though we destroy the actual object earlier.
	// This is actually not really needed for the test, but simulates what would happen with real objects.
	alignas(BadAccessGuardShadow) char shadowAlignedStorage[sizeof(BadAccessGuardShadow)];

	// Create the shadow object in the aligned storage
	BadAccessGuardShadow* shadowPtr = new (&shadowAlignedStorage) BadAccessGuardShadow;

	// Reference to the shadow we'll be using.
	BadAccessGuardShadow& shadow = *shadowPtr;

	// Showcase we can detect we're on the same thread
	{
		printf("Testing read during write on the same thread, output:\n");

		BadAccessGuardWrite writeg{ shadow };
		BadAccessGuardRead readg{ shadow };
	}

	// Now for the actual race from multiple threads
	using namespace std::chrono_literals;
	
	{
		printf("\n\nTesting read during write on different threads, output:\n");

		std::thread otherthread([&] {
#ifdef _WIN32
			SetThreadDescription(GetCurrentThread(), L"ØUnsafe WriterØ");
#endif
			BadAccessGuardWrite writeg{ shadow };
			// Simulate a long write, so that the main thread can attempt to read during this time
			std::this_thread::sleep_for(3s);
			});
		// Wait a bit so that the otherthread has enough time to spawn and go to sleep
		std::this_thread::sleep_for(1s);
		// Assuming the other thread has been scheduled. No sync on purpose since we want to see if our code could detect the race conditon.
		BadAccessGuardRead readg{ shadow };
		// Wait for the other thread to exit
		otherthread.join();
	}


	{
		printf("\n\nTesting write during write on different threads, output:\n");

		using namespace std::chrono_literals;
		std::thread otherthread([&] {
#ifdef _WIN32
			SetThreadDescription(GetCurrentThread(), L"ØUnsafe WriterØ");
#endif
			BadAccessGuardWrite writeg{ shadow };
			// Simulate a long write, so that the main thread can attempt to write during this time
			std::this_thread::sleep_for(3s);
			});
		// Wait a bit so that the otherthread has enough time to spawn and go to sleep
		std::this_thread::sleep_for(1s);
		// Assuming the other thread has been scheduled. No sync on purpose since we want to see if our code could detect the race conditon.
		BadAccessGuardWrite readg{ shadow };
		// Wait for the other thread to exit
		otherthread.join();
	}

	// Destroy the shadow value
	{
		printf("\n\nDestroying shadow, keeping its storage memory.\n");
		BadAccessGuardDestroy gd{ shadow }; // This would typically be in your object destructor
		shadowPtr->~BadAccessGuardShadow(); // Note we don't actually need to destroy it to simulate our tests
	}

	// Attempt use after free
	{
		printf("\nUse after free:\n");
		BadAccessGuardRead readg{ shadow };
	}

	// Corruption, set an invalid state
	{
		printf("\n\nUse after corruption:\n");
		memset(&shadowAlignedStorage, 0xDD ,sizeof(shadowAlignedStorage));
		BadAccessGuardRead readg{ shadow };
	}

	return 0;
}
