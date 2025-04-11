/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "reshade_api_device.hpp"
#include <cassert>
#include <unordered_map>

namespace reshade::api
{
	template <typename T, typename... api_object_base>
	class api_object_impl : public api_object_base...
	{
		static_assert(sizeof(T) <= sizeof(uint64_t));

		struct guid_t
		{
			struct hash
			{
				auto operator()(const guid_t &key) const -> size_t
				{
#ifndef _WIN64
					return key.a ^ key.b ^ ((key.c & 0xFF00) | (key.d & 0xFF));
#else
					return key.a ^ (key.b << 1);
#endif
				}
			};
			struct equal
			{
				bool operator()(const guid_t &lhs, const guid_t &rhs) const
				{
#ifndef _WIN64
					return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c && lhs.d == rhs.d;
#else
					return lhs.a == rhs.a && lhs.b == rhs.b;
#endif
				}
			};

#ifndef _WIN64
			uint32_t a, b, c, d;
#else
			uint64_t a, b;
#endif
		};

	public:
		api_object_impl(const api_object_impl &) = delete;
		api_object_impl &operator=(const api_object_impl &) = delete;

		void get_private_data(const uint8_t guid[16], uint64_t *data) const final
		{
			assert(data != nullptr);

			if (_private_data.empty()) // Early-out to avoid crash when this is called after the object was destroyed
			{
				*data = 0;
				return;
			}

			if (const auto it = _private_data.find(*reinterpret_cast<const guid_t *>(guid));
				it != _private_data.end())
				*data = it->second;
			else
				*data = 0;
		}
		void set_private_data(const uint8_t guid[16], const uint64_t data)  final
		{
			if (data != 0)
				_private_data[*reinterpret_cast<const guid_t *>(guid)] = data;
			else
				_private_data.erase(*reinterpret_cast<const guid_t *>(guid));
		}

		uint64_t get_native() const final { return (uint64_t)_orig; }

		T _orig;

	protected:
		template <typename... Args>
		explicit api_object_impl(T orig, Args... args) : api_object_base(std::forward<Args>(args)...)..., _orig(orig) {}
		~api_object_impl()
		{
			// All user data should ideally have been removed before destruction, to avoid leaks
			assert(_private_data.empty());
		}

	private:
		std::unordered_map<guid_t, uint64_t, typename guid_t::hash, typename guid_t::equal> _private_data;
	};
}

template <typename T, size_t STACK_ELEMENTS = 16>
struct temp_mem
{
	explicit temp_mem(size_t elements = STACK_ELEMENTS) : p(stack)
	{
		if (elements > STACK_ELEMENTS)
			p = new T[elements];
	}
	temp_mem(const temp_mem &) = delete;
	temp_mem(temp_mem &&) = delete;
	~temp_mem()
	{
		if (p != stack)
			delete[] p;
	}

	temp_mem &operator=(const temp_mem &) = delete;
	temp_mem &operator=(temp_mem &&other_mem) = delete;

	T &operator[](size_t element)
	{
		assert(element < STACK_ELEMENTS || p != stack);

		return p[element];
	}

	T *p, stack[STACK_ELEMENTS];
};
