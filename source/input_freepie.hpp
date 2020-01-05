/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <stdint.h>

struct freepie_io_data
{
	float yaw;
	float pitch;
	float roll;

	float x;
	float y;
	float z;
};

struct freepie_io_shared_data
{
	int32_t data_id;
	freepie_io_data data;
};

/// <summary>
/// Read six degrees of freedom data from the FreePIE IO interface.
/// </summary>
/// <param name="index">The device index to read the data from.</param>
/// <param name="output">A pointer to the object that will receive the data.</param>
/// <returns><c>true</c> if the data was read successfully, <c>false</c> if connection could not be established.</returns>
extern "C" bool freepie_io_read(uint32_t index, freepie_io_data *output);
