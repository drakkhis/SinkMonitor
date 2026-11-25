#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <vector>

namespace SinkMonitorCore
{
/** Length of the rolling rate window in 1-second buckets. */
inline constexpr std::size_t kRollingWindowSeconds = 60;

/**
 * Fixed-size 60-second ring buffer tracking exact rolling Items/min,
 * session totals, and lifetime totals with zero floating-point drift.
 */
struct FItemRateRingBuffer
{
	std::array<int64_t, kRollingWindowSeconds> Buckets{};
	std::size_t CurrentIndex = 0;
	int64_t RunningSum = 0;
	int64_t SessionTotal = 0;
	int64_t LifetimeTotal = 0;
	double AccumulatedSeconds = 0.0;

	/** Records Count sunk items into the active 1-second bucket and cumulative totals. */
	void RecordSunk(int64_t Count = 1)
	{
		if (Count <= 0)
		{
			return;
		}
		Buckets[CurrentIndex] += Count;
		RunningSum += Count;
		SessionTotal += Count;
		LifetimeTotal += Count;
	}

	/** Advances the ring buffer by WholeSeconds, expiring stale buckets from RunningSum. */
	void AdvanceWholeSeconds(int32_t WholeSeconds)
	{
		if (WholeSeconds <= 0)
		{
			return;
		}

		if (static_cast<std::size_t>(WholeSeconds) >= kRollingWindowSeconds)
		{
			Buckets.fill(0);
			RunningSum = 0;
			CurrentIndex = 0;
			return;
		}

		for (int32_t Step = 0; Step < WholeSeconds; ++Step)
		{
			CurrentIndex = (CurrentIndex + 1) % kRollingWindowSeconds;
			RunningSum -= Buckets[CurrentIndex];
			Buckets[CurrentIndex] = 0;
		}
	}

	/** Accumulates frame DeltaSeconds and advances whole 1-second buckets cleanly. */
	void Tick(double DeltaSeconds)
	{
		if (DeltaSeconds <= 0.0 || !std::isfinite(DeltaSeconds))
		{
			return;
		}

		AccumulatedSeconds += DeltaSeconds;
		const double WholeFloor = std::floor(AccumulatedSeconds);
		if (WholeFloor < 1.0)
		{
			return;
		}

		AccumulatedSeconds -= WholeFloor;
		if (WholeFloor >= static_cast<double>(kRollingWindowSeconds))
		{
			Buckets.fill(0);
			RunningSum = 0;
			CurrentIndex = 0;
			return;
		}

		AdvanceWholeSeconds(static_cast<int32_t>(WholeFloor));
	}

	/** Returns the exact rolling 60-second rate in Items/min. */
	double GetItemsPerMinute() const
	{
		return static_cast<double>(RunningSum);
	}

	/** Returns total items sunk during the current play session. */
	int64_t GetSessionTotal() const
	{
		return SessionTotal;
	}

	/** Returns lifetime items sunk across saves. */
	int64_t GetLifetimeTotal() const
	{
		return LifetimeTotal;
	}

	/** Restores persisted lifetime total (clamped non-negative). */
	void SetLifetimeTotal(int64_t Value)
	{
		LifetimeTotal = Value > 0 ? Value : 0;
	}

	/** Clears rolling rate and session total while preserving LifetimeTotal. */
	void ResetSession()
	{
		Buckets.fill(0);
		CurrentIndex = 0;
		RunningSum = 0;
		SessionTotal = 0;
		AccumulatedSeconds = 0.0;
	}

	/** Resets rolling rate, session total, and lifetime total. */
	void ResetAll()
	{
		ResetSession();
		LifetimeTotal = 0;
	}
};

/**
 * Multi-key container mapping item or source identifiers to 60-second rate ring buffers.
 */
template <typename KeyType, typename Hash = std::hash<KeyType>>
class TSinkRateTracker
{
public:
	/** Records Count sunk items for Key. */
	void RecordSunk(const KeyType& Key, int64_t Count = 1)
	{
		if (Count <= 0)
		{
			return;
		}
		Entries[Key].RecordSunk(Count);
	}

	/** Advances every tracked ring buffer by DeltaSeconds. */
	void Tick(double DeltaSeconds)
	{
		if (DeltaSeconds <= 0.0 || !std::isfinite(DeltaSeconds))
		{
			return;
		}
		for (auto& Pair : Entries)
		{
			Pair.second.Tick(DeltaSeconds);
		}
	}

	/** Finds an existing buffer for Key, or returns nullptr if unseen. */
	const FItemRateRingBuffer* Find(const KeyType& Key) const
	{
		const auto It = Entries.find(Key);
		return It != Entries.end() ? &It->second : nullptr;
	}

	/** Returns mutable buffer for Key, default-constructing one if absent. */
	FItemRateRingBuffer& GetOrAdd(const KeyType& Key)
	{
		return Entries[Key];
	}

	/** Returns all keys whose rolling 60-second rate is currently above zero. */
	std::vector<KeyType> GetActiveKeys() const
	{
		std::vector<KeyType> Active;
		for (const auto& Pair : Entries)
		{
			if (Pair.second.GetItemsPerMinute() > 0.0)
			{
				Active.push_back(Pair.first);
			}
		}
		return Active;
	}

	/** Clears rolling rates and session totals across all keys while keeping LifetimeTotals. */
	void ResetSession()
	{
		for (auto& Pair : Entries)
		{
			Pair.second.ResetSession();
		}
	}

	/** Removes all tracked keys and buffers. */
	void Clear()
	{
		Entries.clear();
	}

	/** Returns read-only view of all tracked entries. */
	const std::unordered_map<KeyType, FItemRateRingBuffer, Hash>& GetEntries() const
	{
		return Entries;
	}

private:
	std::unordered_map<KeyType, FItemRateRingBuffer, Hash> Entries;
};
} // namespace SinkMonitorCore
