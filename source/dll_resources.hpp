/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "resource.h"
#include <string>

namespace reshade::resources
{
	struct data_resource
	{
		size_t data_size;
		const void *data;
	};

	/// <summary>
	/// Loads the raw data associated with the specified <paramref name="id"/> from the DLL resources.
	/// </summary>
	data_resource load_data_resource(unsigned short id);

	/// <summary>
	/// Loads the localized string associated with the specified <paramref name="id"/> from the DLL resources.
	/// </summary>
	std::string load_string(unsigned short id);
	template <unsigned short id>
	inline std::string load_string() { return load_string(id); }
}
