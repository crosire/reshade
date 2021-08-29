/*
 * Copyright (C) 2019 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <shared_mutex>
#include <unordered_map>

class ptr_hash
{
public:
	inline size_t operator()(uint64_t val) const
	{
		// Identity hash with some bits shaved off due to pointer alignment
		return static_cast<size_t>(val >> 4);
	}
};

/// <summary>
/// A simple thread-safe hash map, guarded by a shared mutex.
/// </summary>
template <typename TKey, typename TValue, typename Hash = std::hash<TKey>>
class locked_hash_map : std::unordered_map<TKey, TValue, Hash>
{
public:
	/// <summary>
	/// Gets a copy of the value associated with the specified <paramref name="key"/>.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns>A copy of the value.</returns>
	__forceinline TValue at(TKey key) const
	{
		const std::shared_lock<std::shared_mutex> lock(_mutex);

		return unordered_map::find(key)->second;
	}

	/// <summary>
	/// Adds the specified key-value pair to the map.
	/// </summary>
	/// <param name="key">The key to add.</param>
	/// <param name="args">The constructor arguments to use for creation.</param>
	/// <returns><c>true</c> if the key-value pair was added, <c>false</c> if the key already existed.</returns>
	template <typename... Args>
	bool emplace(TKey key, Args... args)
	{
		const std::unique_lock<std::shared_mutex> lock(_mutex);

		return unordered_map::emplace(key, std::forward<Args>(args)...).second;
	}

	/// <summary>
	/// Removes the value associated with the specified <paramref name="key"/> from the map.
	/// </summary>
	/// <param name="key">The key to look up.</param>
	/// <returns><c>true</c> if the key existed and was removed, <c>false</c> otherwise.</returns>
	bool erase(TKey key)
	{
		const std::unique_lock<std::shared_mutex> lock(_mutex);

		return unordered_map::erase(key) != 0;
	}

	/// <summary>
	/// Removes all keys that are associated with the specified <paramref name="value"/> from the map.
	/// </summary>
	/// <param name="value">The value to compare with.</param>
	void erase_values(TValue value)
	{
		const std::unique_lock<std::shared_mutex> lock(_mutex);

		for (auto it = unordered_map::begin(); it != unordered_map::end();)
		{
			if (it->second == value)
				it = unordered_map::erase(it);
			else
				++it;
		}
	}

	/// <summary>
	/// Clears the entire map.
	/// </summary>
	void clear()
	{
		const std::unique_lock<std::shared_mutex> lock(_mutex);

		unordered_map::clear();
	}

private:
	mutable std::shared_mutex _mutex;
};

/// <summary>
/// A simple thread-safe hash map for 64-bit handles, guarded by a shared mutex.
/// </summary>
template <typename TValue>
using locked_ptr_hash_map = locked_hash_map<uint64_t, TValue, ptr_hash>;
