/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <cstdint>

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
/// Reads six degrees of freedom data from the FreePIE IO interface.
/// </summary>
/// <param name="index">Device index to read the data from.</param>
/// <param name="output">Pointer to the object that will receive the data.</param>
/// <returns><see langword="true"/> if the data was read successfully, or <see langword="false"/> if connection could not be established.</returns>
extern "C" bool freepie_io_read(uint32_t index, freepie_io_data *output);
