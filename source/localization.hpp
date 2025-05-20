/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

namespace reshade::resources
{
	struct localized_string
	{
		std::string data;

		explicit localized_string(std::string &&data) : data(std::move(data)) {}

		operator const char *() const
		{
			return data.c_str();
		}
		operator std::string &&()
		{
			return std::move(data);
		}

		friend localized_string operator+(const char *prefix, localized_string &&string)
		{
			string.data = prefix + string.data;
			return string;
		}
		friend localized_string operator+(localized_string &&string, const char *postfix)
		{
			string.data += postfix;
			return string;
		}
	};
}

#if RESHADE_LOCALIZATION

#include "dll_resources.hpp"

constexpr uint16_t compute_crc16(const char *data, size_t size)
{
	uint16_t crc = 0;
	while (size--)
	{
		crc ^= *data++;
		for (int k = 0; k < 8; k++)
			crc = crc & 1 ? (crc >> 1) ^ 0xa001 : crc >> 1;
	}
	return crc;
}

#define _(message) reshade::resources::localized_string(reshade::resources::load_string<compute_crc16(message, sizeof(message) - 1)>())

#else

#define _(message) reshade::resources::localized_string(message)

#endif
