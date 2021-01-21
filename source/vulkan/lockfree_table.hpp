/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <atomic>
#include <utility>

/// <summary>
/// A simple lock-free linear search table.
/// The key values "one" and "zero" hold a special meaning (see <see cref="no_value"/> and <see cref="update_value"/>), so do not use them.
/// </summary>
template <typename TKey, typename TValue, size_t MAX_ENTRIES>
class lockfree_table
{
public:
	~lockfree_table()
	{
		clear(); // Free all pointers
	}

	/// <summary>
	/// Special key indicating that the entry is empty.
	/// </summary>
	static constexpr TKey no_value = (TKey)0;
	/// <summary>
	/// Special key indicating that the entry is currently being updated.
	/// </summary>
	static constexpr TKey update_value = (TKey)1;

	/// <summary>
	/// Gets the value associated with the specified <paramref name="key"/>.
	/// This is a weak look up and may fail if another thread is erasing a value at the same time.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns>A reference to the associated value.</returns>
	TValue &at(TKey key) const
	{
		assert(key != no_value && key != update_value);

		size_t start_index = 0;
		// If the table is really big, reduce search time by using hash as start index
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			if (_data[i].first.load(std::memory_order_acquire) == key)
			{
				// The pointer is guaranteed to be value at this point, or else key would have been in update mode
				return *_data[i].second;
			}
		}

		assert(false);
		return default_value(); // Fall back if table is key does not exist
	}

	/// <summary>
	/// Adds a new key-value pair to the table.
	/// </summary>
	/// <param name="key">The key to add.</param>
	/// <returns>A reference to the newly added value.</returns>
	TValue &emplace(TKey key)
	{
		assert(key != no_value && key != update_value);

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		// Create a pointer to the new value
		TValue *new_value = new TValue();

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			// Load and check before doing an expensive CAS
			if (TKey test_key = _data[i].first.load(std::memory_order_relaxed);
				test_key == no_value && // Check if the entry is empty and then do the CAS to occupy it
				_data[i].first.compare_exchange_strong(test_key, update_value, std::memory_order_relaxed))
			{
				_data[i].second = new_value;

				// Now that the new value is stored, make this entry available
				_data[i].first.store(key, std::memory_order_release);

				return *new_value;
			}
		}

		delete new_value;

		assert(false);
		return default_value(); // Fall back if table is full
	}
	/// <summary>
	/// Adds the specified key-value pair to the table.
	/// </summary>
	/// <param name="key">The key to add.</param>
	/// <param name="value">The value to add.</param>
	/// <returns>A reference to the newly added value.</returns>
	TValue &emplace(TKey key, const TValue &value)
	{
		assert(key != no_value && key != update_value);

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		// Create a pointer to the new value using copy construction
		TValue *new_value = new TValue(value);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			if (TKey test_key = _data[i].first.load(std::memory_order_relaxed);
				test_key == no_value &&
				_data[i].first.compare_exchange_strong(test_key, update_value, std::memory_order_relaxed))
			{
				_data[i].second = new_value;

				_data[i].first.store(key, std::memory_order_release);

				return *new_value;
			}
		}

		delete new_value;

		assert(false);
		return default_value(); // Fall back if table is full
	}

	/// <summary>
	/// Removes the value associated with the specified <paramref name="key"/> from the table.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns><c>true</c> if the key existed and was removed, <c>false</c> otherwise.</returns>
	bool erase(TKey key)
	{
		if (key == no_value || key == update_value) // Cannot remove special keys
			return false;

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			// Load and check before doing an expensive CAS
			if (TKey test_key = _data[i].first.load(std::memory_order_relaxed);
				test_key == key)
			{
				// Get the value before freeing the entry up for other threads to fill again
				TValue *const old_value = _data[i].second;

				if (_data[i].first.compare_exchange_strong(test_key, no_value, std::memory_order_relaxed))
				{
					delete old_value;
					return true;
				}
			}
		}

		return false;
	}
	/// <summary>
	/// Removes and returns the value associated with the specified <paramref name="key"/> from the table.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <param name="value">The value associated with that key.</param>
	/// <returns><c>true</c> if the key existed and was removed, <c>false</c> otherwise.</returns>
	bool erase(TKey key, TValue &value)
	{
		if (key == no_value || key == update_value)
			return false;

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			if (TKey test_key = _data[i].first.load(std::memory_order_relaxed);
				test_key == key)
			{
				TValue *const old_value = _data[i].second;

				if (_data[i].first.compare_exchange_strong(test_key, no_value, std::memory_order_relaxed))
				{
					// Move value to output argument and delete its pointer (which is no longer in use now)
					value = std::move(*old_value);

					delete old_value;
					return true;
				}
			}
		}

		return false;
	}

	/// <summary>
	/// Clears the entire table and deletes all keys.
	/// Note that another thread may add new values while this operation is in progress, so do not rely on it.
	/// </summary>
	void clear()
	{
		for (size_t i = 0; i < MAX_ENTRIES; ++i)
		{
			TValue *const old_value = _data[i].second;

			// Clear this entry so it can be used again
			if (TKey current_key = _data[i].first.exchange(no_value);
				current_key != no_value && current_key != update_value) // If this in update mode, we can assume the thread updating will reset the key to its intended value
			{
				// Delete any value attached to the entry, but only if there was one to begin with
				delete old_value;
			}
		}
	}

private:
	static inline TValue &default_value()
	{
		// Make default value thread local, so no data races occur after multiple threads failed to access a value
		static thread_local TValue _ = {}; return _;
	}

	std::pair<std::atomic<TKey>, TValue *> _data[MAX_ENTRIES];
};
