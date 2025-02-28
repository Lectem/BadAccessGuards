#include <thread>
#include <stdio.h>
#include <stdlib.h>
#ifdef _WIN32
# include <Windows.h>
#endif
#include <BadAccessGuards.h>
#include <vector>
#include "GuardedVectorExample.h"
#include <functional>
#include <chrono>

// Simulate any kind of event system where one may register and trigger delegates
static std::vector<std::function<void()>> gEventDelegates;
static void TriggerEvent()
{
    for (const auto& delegate : gEventDelegates)
    {
        delegate();
    }
}

int main()
{
#if !BAD_ACCESS_GUARDS_ENABLE
# error "Can't really test the guards if we don't enable them can we ?"
#endif

    printf("This test should be ran under a debugger as it will trigger breakpoints and crash if no debugger is attached!\n");

    int accum = 0; // Variable to avoid optimizations

    {
        printf("\nTesting write during write on the same thread, output:\n");
        // A class that somehow triggers the event on construction
        struct RecursiveBehaviour
        { 
            RecursiveBehaviour()
            {
                // You are actually inside the `ExampleGuardedVector::emplace_back()`!
                TriggerEvent();
            }
        };
    
        ExampleGuardedVector<RecursiveBehaviour> vector;
        // Somewhere you setup a delegate function clearing the vector
        gEventDelegates.push_back([&]() {
            vector.clear();
            });
    
        // Then you add some item that triggers the event delegates (could be a signal, event, etc...)
        // The issue is that while modifying the vector itself (you're building the item inside it!), you're also asking to clear it (due to the delegate).
        vector.emplace_back();
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
