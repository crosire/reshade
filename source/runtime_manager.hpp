/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include "reshade_api_device.hpp"

namespace reshade
{
	bool create_effect_runtime(api::swapchain *swapchain, api::command_queue *graphics_queue, bool is_vr = false);
	bool destroy_effect_runtime(api::swapchain *swapchain);

	bool init_effect_runtime(api::swapchain *swapchain);
	bool reset_effect_runtime(api::swapchain *swapchain);
	bool present_effect_runtime(api::swapchain *swapchain, api::command_queue *present_queue);
}
