/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <stdint.h>

struct freepie_io_6dof_data
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
	freepie_io_6dof_data data;
};

bool freepie_io_6dof_read(uint32_t index, freepie_io_6dof_data *output);
