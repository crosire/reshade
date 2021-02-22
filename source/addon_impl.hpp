/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <vector>

namespace reshade::api
{
	template <typename T, typename... BASE>
	class api_object_impl : public BASE...
	{
	public:
		explicit api_object_impl(T orig) : _orig(orig) {}

		api_object_impl(const api_object_impl &) = delete;
		api_object_impl &operator=(const api_object_impl &) = delete;

		bool get_data(const uint8_t guid[16], void **ptr) const override
		{
			for (auto it = _data_entries.begin(); it != _data_entries.end(); ++it)
			{
				if (it->guid[0] == reinterpret_cast<const uint64_t *>(guid)[0] &&
					it->guid[1] == reinterpret_cast<const uint64_t *>(guid)[1])
				{
					*ptr = it->data;
					return true;
				}
			}
			return false;
		}
		void set_data(const uint8_t guid[16], void * const ptr) override
		{
			for (auto it = _data_entries.begin(); it != _data_entries.end(); ++it)
			{
				if (it->guid[0] == reinterpret_cast<const uint64_t *>(guid)[0] &&
					it->guid[1] == reinterpret_cast<const uint64_t *>(guid)[1])
				{
					if (ptr == nullptr)
						_data_entries.erase(it);
					else
						it->data = ptr;
					return;
				}
			}

			if (ptr != nullptr)
			{
				_data_entries.push_back({ ptr, {
					reinterpret_cast<const uint64_t *>(guid)[0],
					reinterpret_cast<const uint64_t *>(guid)[1] } });
			}
		}

		uint64_t get_native_object() const override { return (uint64_t)_orig; }

		T _orig;

	private:
		struct entry
		{
			void *data;
			uint64_t guid[2];
		};

		std::vector<entry> _data_entries;
	};
}
