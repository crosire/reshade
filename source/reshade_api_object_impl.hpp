/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "reshade_api_device.hpp"
#include <vector>
#include <cassert>

namespace reshade
{
	void create_effect_runtime(api::swapchain *swapchain, api::command_queue *graphics_queue, bool is_vr = false);
	void destroy_effect_runtime(api::swapchain *swapchain);

	void init_effect_runtime(api::swapchain *swapchain);
	void reset_effect_runtime(api::swapchain *swapchain);
	void present_effect_runtime(api::swapchain *swapchain, api::command_queue *present_queue);
}

namespace reshade::api
{
	template <typename T, typename... api_object_base>
	class api_object_impl : public api_object_base...
	{
		static_assert(sizeof(T) <= sizeof(uint64_t));

	public:
		api_object_impl(const api_object_impl &) = delete;
		api_object_impl &operator=(const api_object_impl &) = delete;

		void get_private_data(const uint8_t guid[16], uint64_t *data) const final
		{
			assert(data != nullptr);

			for (auto it = _private_data.begin(); it != _private_data.end(); ++it)
			{
				if (std::memcmp(it->guid, guid, 16) == 0)
				{
					*data = it->data;
					return;
				}
			}

			*data = 0;
		}
		void set_private_data(const uint8_t guid[16], const uint64_t data)  final
		{
			for (auto it = _private_data.begin(); it != _private_data.end(); ++it)
			{
				if (std::memcmp(it->guid, guid, 16) == 0)
				{
					if (data != 0)
						it->data = data;
					else
						_private_data.erase(it);
					return;
				}
			}

			if (data != 0)
			{
				_private_data.push_back({ data, {
					reinterpret_cast<const uint64_t *>(guid)[0],
					reinterpret_cast<const uint64_t *>(guid)[1] } });
			}
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
		struct private_data
		{
			uint64_t data;
			uint64_t guid[2];
		};

		std::vector<private_data> _private_data;
	};
}

template <typename T, size_t STACK_ELEMENTS = 16>
struct temp_mem
{
	explicit temp_mem(size_t elements = STACK_ELEMENTS)
	{
		if (elements > STACK_ELEMENTS)
			p = new T[elements];
		else
			p = stack;
	}
	~temp_mem()
	{
		if (p != stack)
			delete[] p;
	}

	T &operator[](size_t element)
	{
		assert(element < STACK_ELEMENTS || p != stack);

		return p[element];
	}

	T *p, stack[STACK_ELEMENTS];
};
