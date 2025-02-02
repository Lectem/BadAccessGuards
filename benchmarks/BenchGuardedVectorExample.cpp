#include "../examples/GuardedVectorExample.h"

#include <nanobench.h>
#include <chrono>

#include <vector>

#if defined(__has_feature) // Clang
#  if __has_feature(thread_sanitizer)
#    define USING_THREAD_SANITIZER 1
#  else
#    define USING_THREAD_SANITIZER 0
#  endif
#elif defined(__SANITIZE_THREAD__) // GCC
#  define USING_THREAD_SANITIZER 1
#else
#  define USING_THREAD_SANITIZER 0
#endif

using namespace std::chrono_literals;
const auto minEpoch = 100ms;

template<typename T>
int BenchVector(ankerl::nanobench::Bench& bench, bool withNoReserve)
{
#ifdef NDEBUG
	const size_t nbPushBacksPerIteration[] = { 1'000, 100'000, 10'000'000 };
#else
	const size_t nbPushBacksPerIteration[] = { 1'000 };
#endif
	uint64_t x = 1;
	{
		std::vector<T> vector;
		for (size_t size : nbPushBacksPerIteration)
		{
			vector.reserve(size); // Don't measure allocator
			bench.complexityN(size)
				.minEpochTime(minEpoch)
				.run("std::vector.push_back", [&] {
				for (int i = 0; i < size; i++)
				{
					vector.push_back({ x });
				}
				ankerl::nanobench::doNotOptimizeAway(x += vector.size());
				vector.clear();
					});
		}
	}

#if !USING_THREAD_SANITIZER
#ifndef NDEBUG // Only gives different perf in debug builds
	{
		ExampleGuardedVector<T> guardedvector;
		for (size_t size : nbPushBacksPerIteration)
		{
			guardedvector.reserve(size); // Don't measure allocator
			bench.complexityN(size)
				.minEpochTime(minEpoch)
				.run("guardedvector.push_back_noguard", [&] {
				for (int i = 0; i < size; i++)
				{
					// This is benched to demonstrate that the guard is not much more expensive thant a simple forwarding call in debug...
					guardedvector.push_back_noguard({ x });
				}
				ankerl::nanobench::doNotOptimizeAway(x += guardedvector.size());
				guardedvector.clear();
					});
		}
	}
#endif
	{
		ExampleGuardedVector<T> guardedvector;
		for (size_t size : nbPushBacksPerIteration)
		{
			guardedvector.reserve(size); // Don't measure allocator
			bench.complexityN(size)
				.minEpochTime(minEpoch)
				.run("guardedvector.push_back", [&] {
				for (int i = 0; i < size; i++)
				{
					guardedvector.push_back({ x });
				}
				ankerl::nanobench::doNotOptimizeAway(x += guardedvector.size());
				guardedvector.clear();
					});
		}
	}
#endif //!USING_THREAD_SANITIZER
	if (withNoReserve)
	{
		{
			std::vector<T> vector;
			for (size_t size : nbPushBacksPerIteration)
			{
				bench.complexityN(size)
					.minEpochTime(minEpoch)
					.run("std::vector.push_back - noreserve", [&] {
					for (int i = 0; i < size; i++)
					{
						vector.push_back({ x });
					}
					ankerl::nanobench::doNotOptimizeAway(x += vector.size());
					vector.clear();
					vector.shrink_to_fit();
						});
			}
		}

#if !USING_THREAD_SANITIZER
#ifndef NDEBUG // Only gives different perf in debug builds
		{
			ExampleGuardedVector<T> guardedvector;
			for (size_t size : nbPushBacksPerIteration)
			{
				guardedvector.reserve(size); // Don't measure allocator
				bench.complexityN(size)
					.minEpochTime(minEpoch)
					.run("guardedvector.push_back_noguard - noreserve", [&] {
					for (int i = 0; i < size; i++)
					{
						// This is benched to demonstrate that the guard is not much more expensive thant a simple forwarding call in debug...
						guardedvector.push_back_noguard({ x });
					}
					ankerl::nanobench::doNotOptimizeAway(x += guardedvector.size());
					guardedvector.clear();
					guardedvector.shrink_to_fit();
						});
			}
		}
#endif
		{
			ExampleGuardedVector<T> guardedvector;
			for (size_t size : nbPushBacksPerIteration)
			{
				bench.complexityN(size)
					.minEpochTime(minEpoch)
					.run("guardedvector.push_back - noreserve", [&] {
					for (int i = 0; i < size; i++)
					{
						guardedvector.push_back({ x });
					}
					ankerl::nanobench::doNotOptimizeAway(x += guardedvector.size());
					guardedvector.clear();
					guardedvector.shrink_to_fit();
						});
			}
		}
#endif //!USING_THREAD_SANITIZER
	}
	return x;
}

int main() {
	
	int x = 0;

	x += BenchVector<uint64_t>(ankerl::nanobench::Bench().title("Vector of uint64_t"), true);
	struct Payload16B {
		uint64_t storage[2];
	};
	x += BenchVector<Payload16B>(ankerl::nanobench::Bench().title("Vector of uint64_t*2"), false);
	struct Payload32B {
		uint64_t storage[4];
	};
	x += BenchVector<Payload32B>(ankerl::nanobench::Bench().title("Vector of uint64_t*4"), false);
	struct PayloadString {
		PayloadString(uint64_t v) {
			(void)v;
			storage += char(v);
		}
		std::string storage;
	};
	x += BenchVector<PayloadString>(ankerl::nanobench::Bench().title("Vector of std::string"), false);
	return x;
}