/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <atomic>
#include <vector>

/// <summary>
/// A simple lock-free linear search table.
/// A key value one zero holds a special meaning, so do not use it.
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
	/// Gets the value associated with the specified <paramref name="key"/>.
	/// This is a weak look up and may fail if another thread is erasing a value at the same time.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns>A reference to the associated value.</returns>
	TValue &at(TKey key) const
	{
		assert(key != 0);

		size_t start_index = 0;
		// If the table is really big, reduce search time by using hash as start index
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			if (_data[i].first == key)
			{
				TValue *const value = _data[i].second;
				if (value == nullptr)
					continue; // Other thread may have created this entry, but not set the value yet
				else
					return *value;
			}
		}

		return _default; // Fall back if table is key does not exist
	}

	/// <summary>
	/// Adds a new key-value pair to the table.
	/// </summary>
	/// <param name="key">The key to add.</param>
	/// <returns>A reference to the newly added value.</returns>
	TValue &emplace(TKey key)
	{
		assert(key != 0);

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			if (TKey test_value = 0;
				_data[i].first.compare_exchange_strong(test_value, key))
			{
				// Create a pointer to the new value
				TValue *new_value = new TValue();
				// Atomically store pointer in this entry and return it
				_data[i].second = new_value;
				return *new_value;
			}
		}

		return _default; // Fall back if table is full
	}
	/// <summary>
	/// Adds the specified key-value pair to the table.
	/// </summary>
	/// <param name="key">The key to add.</param>
	/// <param name="value">The value to add.</param>
	/// <returns>A reference to the newly added value.</returns>
	TValue &emplace(TKey key, const TValue &value)
	{
		assert(key != 0);

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			if (TKey test_value = 0;
				_data[i].first.compare_exchange_strong(test_value, key))
			{
				// Create a pointer to the new value
				TValue *new_value = new TValue(value);
				// Atomically store pointer in this entry and return it
				_data[i].second = new_value;
				return *new_value;
			}
		}

		return _default; // Fall back if table is full
	}

	/// <summary>
	/// Removes the value associated with the specified <paramref name="key"/> from the table.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns><c>true</c> if the key existed and was removed, <c>false</c> otherwise.</returns>
	bool erase(TKey key)
	{
		if (key == 0) // Zero has a special meaning, so cannot remove it
			return false;

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			TValue *const old_value = _data[i].second.exchange(nullptr);

			if (TKey test_value = key;
				_data[i].first.compare_exchange_strong(test_value, 0))
			{
				delete old_value;
				return true;
			}

			_data[i].second = old_value;
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
		if (key == 0) // Zero has a special meaning, so cannot remove it
			return false;

		size_t start_index = 0;
		if constexpr (MAX_ENTRIES > 512)
			start_index = std::hash<TKey>()(key) % (MAX_ENTRIES / 2);

		for (size_t i = start_index; i < MAX_ENTRIES; ++i)
		{
			// Fetch current value before doing CAS below, since other thread may be using this entry to store new data immediately
			TValue *const old_value = _data[i].second.exchange(nullptr);

			if (TKey test_value = key;
				_data[i].first.compare_exchange_strong(test_value, 0))
			{
				// Move value to output argument
				value = std::move(*old_value);
				// Then delete pointer to old value and return
				delete old_value;
				return true;
			}

			_data[i].second = old_value;
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
			TValue *const old_value = _data[i].second.exchange(nullptr);

			// Clear this entry so it can be used again
			if (_data[i].first.exchange(0))
			{
				// Delete any value attached to the entry, but only if there was one to begin with
				delete old_value;
			}
		}
	}

	/// <summary>
	/// Gets or adds a value for the specified <paramref name="key"/>, depending on whether it already exists or not.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns>A reference to the associated value.</returns>
	inline TValue &operator[](TKey key)
	{
		TValue *const value = &at(key);
		if (value != &_default)
			return *value;
		return emplace(key);
	}

private:
	mutable TValue _default = {};
	std::pair<std::atomic<TKey>, std::atomic<TValue *>> _data[MAX_ENTRIES];
};
