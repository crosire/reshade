/*
 * Copyright (C) 2024 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "runtime.hpp"
#include "runtime_manager.hpp"
#include "ini_file.hpp"
#include <cassert>
#include <shared_mutex>
#include <unordered_set>

static std::shared_mutex s_runtime_config_names_mutex;
static std::unordered_set<std::string> s_runtime_config_names;

void reshade::create_effect_runtime(api::swapchain *swapchain, api::command_queue *graphics_queue, bool is_vr)
{
	if (graphics_queue == nullptr || swapchain->get_private_data<reshade::runtime>() != nullptr)
		return;

	assert((graphics_queue->get_type() & api::command_queue_type::graphics) != 0);

	// Try to find a unique configuration name for this effect runtime instance
	std::string config_name = "ReShade";
	if (is_vr)
		config_name += "VR";
	{
		const std::string config_name_base = config_name;

		const std::unique_lock<std::shared_mutex> lock(s_runtime_config_names_mutex);

		if (size_t max_runtimes = std::numeric_limits<size_t>::max();
			global_config().get("INSTALL", "MaxEffectRuntimes", max_runtimes) &&
			s_runtime_config_names.size() >= max_runtimes)
				return;

		for (int attempt = 1; attempt < 100 && s_runtime_config_names.find(config_name) != s_runtime_config_names.end(); ++attempt)
			config_name = config_name_base + std::to_string(attempt + 1);

		assert(s_runtime_config_names.find(config_name) == s_runtime_config_names.end());
		s_runtime_config_names.insert(config_name);
	}

	const ini_file &config = ini_file::load_cache(g_reshade_base_path / std::filesystem::u8path(config_name + ".ini"));
	if (config.get("GENERAL", "Disable"))
		return;

	swapchain->create_private_data<reshade::runtime>(swapchain, graphics_queue, config.path(), is_vr);
}
void reshade::destroy_effect_runtime(api::swapchain *swapchain)
{
	if (const auto runtime = swapchain->get_private_data<reshade::runtime>())
	{
		// Free up the configuration name of this effect runtime instance for reuse
		const std::unique_lock<std::shared_mutex> lock(s_runtime_config_names_mutex);

		s_runtime_config_names.erase(runtime->get_config_path().stem().u8string());
	}

	swapchain->destroy_private_data<reshade::runtime>();
}

void reshade::init_effect_runtime(api::swapchain *swapchain)
{
	if (const auto runtime = swapchain->get_private_data<reshade::runtime>())
		runtime->on_init();
}
void reshade::reset_effect_runtime(api::swapchain *swapchain)
{
	if (const auto runtime = swapchain->get_private_data<reshade::runtime>())
		runtime->on_reset();
}
void reshade::present_effect_runtime(api::swapchain *swapchain, reshade::api::command_queue *present_queue)
{
	if (const auto runtime = swapchain->get_private_data<reshade::runtime>())
		runtime->on_present(present_queue);
}
