/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "resource.h"

namespace reshade::resources
{
	struct data_resource
	{
		size_t data_size;
		const void *data;
	};

	struct image_resource : public data_resource
	{
		unsigned int width, height, bits_per_pixel;
	};

	/// <summary>
	/// Loads the raw data associated with the specified <paramref name="id"/> from the DLL resources.
	/// </summary>
	data_resource load_data_resource(unsigned int id);
	/// <summary>
	/// Loads the bitmap data associated with the specified <paramref name="id"/> from the DLL resources.
	/// </summary>
	image_resource load_image_resource(unsigned int id);
}
