/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "resource.h"
#include <string>
#include <vector>

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
	/// <param name="id">Resource identifier of the raw data resource.</param>
	data_resource load_data_resource(unsigned short id);

	/// <summary>
	/// Loads the localized string associated with the specified <paramref name="id"/> from the DLL resources.
	/// </summary>
	/// <param name="id">Resource identifier of the string resource.</param>
	std::string load_string(unsigned short id);
	template <unsigned short id>
	inline std::string load_string() { return load_string(id); }

#if RESHADE_LOCALIZATION
	/// <summary>
	/// Gets the preferred UI language used to look up resources for the current thread.
	/// </summary>
	std::string get_current_language();
	/// <summary>
	/// Changes the language used to look up resources for the current thread.
	/// </summary>
	/// <param name="language">Language name of the new language to set.</param>
	/// <returns>Language name of the language that was set before the change.</returns>
	std::string set_current_language(const std::string &language);

	/// <summary>
	/// Returns a list of languages for which a string table is embeded in the DLL resources.
	/// </summary>
	std::vector<std::string> get_languages();
#endif
}
