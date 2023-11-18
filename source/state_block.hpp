/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "reshade_api_device.hpp"

namespace reshade::api
{
	RESHADE_DEFINE_HANDLE(state_block);

	void create_state_block(api::device *device, state_block *out_state_block);
	void destroy_state_block(api::device *device, state_block state_block);

	void apply_state(api::command_list *cmd_list, state_block state_block);
	void capture_state(api::command_list *cmd_list, state_block state_block);
}
