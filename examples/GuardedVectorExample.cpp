#include <thread>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
#include <Windows.h>
#endif
#include <BadAccessGuards.h>
#include <vector>
#include "GuardedVectorExample.h"

struct RecursiveBehaviour {
	ExampleGuardedVector<RecursiveBehaviour>& vectorRef;
	int& valueRef;
	RecursiveBehaviour(ExampleGuardedVector<RecursiveBehaviour>& v, int& value)
		: vectorRef(v)
		, valueRef(value)
	{
	}

	~RecursiveBehaviour()
	{
		// This is something that sometimes happen when you have a lot of callbacks...
		// Things will try to use the container they are being removed from, 
		// which is sometimes fine, but most often dangerous.
		// Here we're doing a read, but it could be something adding an element !
		valueRef += vectorRef.size();
	}
};

int main()
{
#ifndef BAD_ACCESS_GUARDS_ENABLE
#error "Can't really test the guards if we don't enable them can we ?"
#endif

	printf("This test should be ran under a debugger as it will trigger breakpoints and crash if no debugger is attached!\n");

	int accum = 0; // Variable to avoid optimizations

	{
		ExampleGuardedVector<RecursiveBehaviour> v;
		printf("Testing read during write on the same thread, output:\n");
		v.push_back({ v, accum });
		v.clear();
	}

	using namespace std::chrono_literals;
	{
		printf("\n\nShowcase we do not crash on reads from different threads, there should be no output:\n");

		ExampleGuardedVector<int> vec;
		for (int i = 0; i < 10'000'000; i++)
		{
			vec.push_back(i);
		}


		auto beginTime = std::chrono::steady_clock::now();
		// Run both threads concurrently for 0.5s
		auto endTime = std::chrono::steady_clock::now() + 0.5s;

		std::thread otherthread([&] {
			while (std::chrono::steady_clock::now() < endTime)
			{
				for (int elem : vec) { accum += elem; }
			}
			});
		// Assume other thread will be scheduled before the endTime
		while (std::chrono::steady_clock::now() < endTime)
		{
			for (int elem : vec) { accum += elem; }
		}
		otherthread.join();
	}

	{
		printf("\n\nTesting read during write on different threads, output:\n");

		auto beginTime = std::chrono::steady_clock::now();
		// Run both threads concurrently for 0.5s
		auto endTime = std::chrono::steady_clock::now() + 0.5s;

		ExampleGuardedVector<int> vec;
		std::thread otherthread([&] {
#ifdef _WIN32
			SetThreadDescription(GetCurrentThread(), L"ØUnsafe WriterØ");
#endif
			while (std::chrono::steady_clock::now() < endTime)
			{
				for (int i = 0; i < 10'000'000; i++)
				{
					vec.push_back(i);
				}
				vec.clear();
				vec.shrink_to_fit(); // Force this thread to reallocate a bit
			}
			std::this_thread::sleep_for(3s);
			});
		
		// Assume other thread will be scheduled before the endTime
		while (std::chrono::steady_clock::now() < endTime)
		{
			// We may (or may not) crash while reading since the other thread may be reallocating
			for (int elem : vec)
			{
				accum += elem;
			}
		}

		otherthread.join();
	}
	return accum; // Prevent optimization
}
