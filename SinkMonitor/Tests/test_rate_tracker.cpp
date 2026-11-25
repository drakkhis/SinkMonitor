#include "SinkRateTracker.h"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace
{
void TestSingleBucketAndExactExpiry()
{
	SinkMonitorCore::FItemRateRingBuffer Buffer;
	Buffer.RecordSunk(15);

	assert(Buffer.GetItemsPerMinute() == 15.0);
	assert(Buffer.GetSessionTotal() == 15);
	assert(Buffer.GetLifetimeTotal() == 15);

	// Advance 59 whole seconds: bucket 0 must still be inside the 60-second window.
	for (int32_t Sec = 0; Sec < 59; ++Sec)
	{
		Buffer.Tick(1.0);
	}
	assert(Buffer.GetItemsPerMinute() == 15.0);
	assert(Buffer.GetSessionTotal() == 15);
	assert(Buffer.GetLifetimeTotal() == 15);

	// On the 60th second, bucket 0 wraps and expires cleanly with zero drift.
	Buffer.Tick(1.0);
	assert(Buffer.GetItemsPerMinute() == 0.0);
	assert(Buffer.GetSessionTotal() == 15);
	assert(Buffer.GetLifetimeTotal() == 15);
	std::cout << "PASS: TestSingleBucketAndExactExpiry\n";
}

void TestSteadyStateThroughput()
{
	SinkMonitorCore::FItemRateRingBuffer Buffer;
	// Feed 2 items every 1 second for 180 seconds -> steady 120 items/min.
	for (int32_t Sec = 0; Sec < 180; ++Sec)
	{
		Buffer.RecordSunk(2);
		if (Sec == 59)
		{
			assert(Buffer.GetItemsPerMinute() == 120.0);
		}
		Buffer.Tick(1.0);
		if (Sec >= 59)
		{
			// 59 remaining active buckets of 2 items + newly entered cleared bucket = 118 until next RecordSunk.
			assert(Buffer.GetItemsPerMinute() == 118.0);
		}
	}
	assert(Buffer.GetSessionTotal() == 360);
	assert(Buffer.GetLifetimeTotal() == 360);
	std::cout << "PASS: TestSteadyStateThroughput\n";
}

void TestLargeHitchAndFractionalTick()
{
	SinkMonitorCore::FItemRateRingBuffer Buffer;
	Buffer.SetLifetimeTotal(1000);
	Buffer.RecordSunk(10);

	// Fractional ticks: 4 * 0.25s = 1 full second advance.
	for (int i = 0; i < 4; ++i)
	{
		Buffer.Tick(0.25);
	}
	assert(Buffer.GetItemsPerMinute() == 10.0);

	// Large pause >= 60s clears rolling window while keeping SessionTotal & LifetimeTotal intact.
	Buffer.Tick(125.5);
	assert(Buffer.GetItemsPerMinute() == 0.0);
	assert(Buffer.GetSessionTotal() == 10);
	assert(Buffer.GetLifetimeTotal() == 1010);
	assert(std::abs(Buffer.AccumulatedSeconds - 0.5) < 1e-9);
	std::cout << "PASS: TestLargeHitchAndFractionalTick\n";
}

void TestInvalidInputsIgnored()
{
	SinkMonitorCore::FItemRateRingBuffer Buffer;
	Buffer.RecordSunk(5);
	Buffer.RecordSunk(0);
	Buffer.RecordSunk(-42);
	Buffer.AdvanceWholeSeconds(0);
	Buffer.AdvanceWholeSeconds(-10);
	Buffer.Tick(0.0);
	Buffer.Tick(-5.0);
	Buffer.Tick(std::numeric_limits<double>::infinity());
	Buffer.Tick(std::numeric_limits<double>::quiet_NaN());

	assert(Buffer.GetItemsPerMinute() == 5.0);
	assert(Buffer.GetSessionTotal() == 5);
	assert(Buffer.GetLifetimeTotal() == 5);
	std::cout << "PASS: TestInvalidInputsIgnored\n";
}

void TestMultiKeyTrackerAndSessionReset()
{
	SinkMonitorCore::TSinkRateTracker<std::string> Tracker;
	Tracker.RecordSunk("Desc_IronPlate_C", 4);
	Tracker.RecordSunk("Desc_Screw_C", 12);
	Tracker.RecordSunk("Desc_Ignored_C", 0);

	assert(Tracker.Find("Desc_Ignored_C") == nullptr);
	const auto* Iron = Tracker.Find("Desc_IronPlate_C");
	const auto* Screw = Tracker.Find("Desc_Screw_C");
	assert(Iron != nullptr && Iron->GetItemsPerMinute() == 4.0);
	assert(Screw != nullptr && Screw->GetItemsPerMinute() == 12.0);

	const std::vector<std::string> ActiveBefore = Tracker.GetActiveKeys();
	assert(ActiveBefore.size() == 2);

	Tracker.Tick(60.0);
	assert(Tracker.GetActiveKeys().empty());
	assert(Iron->GetSessionTotal() == 4);
	assert(Screw->GetSessionTotal() == 12);

	Tracker.ResetSession();
	assert(Iron->GetSessionTotal() == 0);
	assert(Iron->GetLifetimeTotal() == 4);
	assert(Screw->GetSessionTotal() == 0);
	assert(Screw->GetLifetimeTotal() == 12);

	Tracker.Clear();
	assert(Tracker.Find("Desc_IronPlate_C") == nullptr);
	std::cout << "PASS: TestMultiKeyTrackerAndSessionReset\n";
}
} // namespace

int main()
{
	TestSingleBucketAndExactExpiry();
	TestSteadyStateThroughput();
	TestLargeHitchAndFractionalTick();
	TestInvalidInputsIgnored();
	TestMultiKeyTrackerAndSessionReset();
	std::cout << "ALL TESTS PASSED\n";
	return 0;
}
