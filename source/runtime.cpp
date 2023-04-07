/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "version.h"
#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "ini_file.hpp"
#include "addon_manager.hpp"
#include "runtime.hpp"
#include "runtime_objects.hpp"
#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include "effect_preprocessor.hpp"
#include "input.hpp"
#include "input_gamepad.hpp"
#include "com_ptr.hpp"
#include "platform_utils.hpp"
#include <set>
#include <thread>
#include <cctype>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <numeric>
#include <fpng.h>
#include <stb_image.h>
#include <stb_image_dds.h>
#include <stb_image_write.h>
#include <stb_image_resize.h>
#include <d3dcompiler.h>

#if RESHADE_FX
bool resolve_path(std::filesystem::path &path, std::error_code &ec)
{
	// First convert path to an absolute path
	// Ignore the working directory and instead start relative paths at the DLL location
	if (!path.is_absolute())
		path = std::filesystem::absolute(g_reshade_base_path / path, ec);
	// Finally try to canonicalize the path too
	if (std::filesystem::path canonical_path = std::filesystem::canonical(path, ec); !ec)
		path = std::move(canonical_path);
	return !ec; // The canonicalization step fails if the path does not exist
}
bool resolve_preset_path(std::filesystem::path &path, std::error_code &ec)
{
	ec.clear();
	// First make sure the extension matches, before diving into the file system
	if (const std::filesystem::path ext = path.extension();
		ext != L".ini" && ext != L".txt")
		return false;
	// A non-existent path is valid for a new preset
	// Otherwise ensure the file has a technique list, which should make it a preset
	return !resolve_path(path, ec) || ini_file::load_cache(path).has({}, "Techniques");
}

static bool find_file(const std::vector<std::filesystem::path> &search_paths, std::filesystem::path &path)
{
	std::error_code ec;
	// Do not have to perform a search if the path is already absolute
	if (path.is_absolute())
		return std::filesystem::exists(path, ec);

	for (std::filesystem::path search_path : search_paths)
	{
		const bool recursive_search = search_path.filename() == L"**";
		if (recursive_search)
			search_path.remove_filename();

		// Append relative file path to absolute search path
		if (std::filesystem::path search_sub_path = search_path / path;
			resolve_path(search_sub_path, ec))
		{
			path = std::move(search_sub_path);
			return true;
		}

		if (recursive_search)
		{
			for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(search_path, std::filesystem::directory_options::skip_permission_denied, ec))
			{
				if (!entry.is_directory(ec))
					continue;

				if (std::filesystem::path search_sub_path = entry / path;
					resolve_path(search_sub_path, ec))
				{
					path = std::move(search_sub_path);
					return true;
				}
			}
		}
	}

	return false;
}
static std::vector<std::filesystem::path> find_files(const std::vector<std::filesystem::path> &search_paths, std::initializer_list<std::filesystem::path> extensions)
{
	std::error_code ec;
	std::vector<std::filesystem::path> files;
	std::vector<std::pair<std::filesystem::path, bool>> resolved_search_paths;

	// First resolve all search paths and ensure they are all unique
	for (std::filesystem::path search_path : search_paths)
	{
		const bool recursive_search = search_path.filename() == L"**";
		if (recursive_search)
			search_path.remove_filename();

		if (resolve_path(search_path, ec))
		{
			if (const auto it = std::find_if(resolved_search_paths.begin(), resolved_search_paths.end(),
					[&search_path](const std::pair<std::filesystem::path, bool> &recursive_search_path) {
						return recursive_search_path.first == search_path;
					});
				it != resolved_search_paths.end())
				it->second |= recursive_search;
			else
				resolved_search_paths.push_back(std::make_pair(std::move(search_path), recursive_search));
		}
		else
		{
			LOG(WARN) << "Failed to resolve search path " << search_path << " with error code " << ec.value() << '.';
		}
	}

	// Then iterate through all files in those search paths and add those with a matching extension
	const auto check_and_add_file = [&extensions, &ec, &files](const std::filesystem::directory_entry &entry) {
		if (!entry.is_directory(ec) &&
			std::find(extensions.begin(), extensions.end(), entry.path().extension()) != extensions.end())
			files.emplace_back(entry); // Construct path from directory entry in-place
	};

	for (const std::pair<std::filesystem::path, bool> &resolved_search_path : resolved_search_paths)
	{
		if (resolved_search_path.second)
			for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(resolved_search_path.first, std::filesystem::directory_options::skip_permission_denied, ec))
				check_and_add_file(entry);
		else
			for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(resolved_search_path.first, std::filesystem::directory_options::skip_permission_denied, ec))
				check_and_add_file(entry);
	}

	return files;
}

static inline int format_color_bit_depth(reshade::api::format value)
{
	switch (value)
	{
	default:
		assert(false);
		return 0;
	case reshade::api::format::b5g6r5_unorm:
	case reshade::api::format::b5g5r5a1_unorm:
	case reshade::api::format::b5g5r5x1_unorm:
		return 5;
	case reshade::api::format::r8g8b8a8_typeless:
	case reshade::api::format::r8g8b8a8_unorm:
	case reshade::api::format::r8g8b8a8_unorm_srgb:
	case reshade::api::format::r8g8b8x8_unorm:
	case reshade::api::format::r8g8b8x8_unorm_srgb:
	case reshade::api::format::b8g8r8a8_typeless:
	case reshade::api::format::b8g8r8a8_unorm:
	case reshade::api::format::b8g8r8a8_unorm_srgb:
	case reshade::api::format::b8g8r8x8_typeless:
	case reshade::api::format::b8g8r8x8_unorm:
	case reshade::api::format::b8g8r8x8_unorm_srgb:
		return 8;
	case reshade::api::format::r10g10b10a2_typeless:
	case reshade::api::format::r10g10b10a2_unorm:
	case reshade::api::format::r10g10b10a2_xr_bias:
	case reshade::api::format::b10g10r10a2_typeless:
	case reshade::api::format::b10g10r10a2_unorm:
		return 10;
	case reshade::api::format::r11g11b10_float:
		return 11;
	case reshade::api::format::r16g16b16a16_typeless:
	case reshade::api::format::r16g16b16a16_float:
		return 16;
	case reshade::api::format::r32g32b32_typeless:
	case reshade::api::format::r32g32b32_float:
	case reshade::api::format::r32g32b32a32_typeless:
	case reshade::api::format::r32g32b32a32_float:
		return 32;
	}
}
#endif

static std::atomic<unsigned int> s_runtime_index = 0;

reshade::runtime::runtime(api::device *device, api::command_queue *graphics_queue) :
	_device(device),
	_graphics_queue(graphics_queue),
	_start_time(std::chrono::high_resolution_clock::now()),
	_last_present_time(_start_time),
	_last_frame_duration(std::chrono::milliseconds(1)),
#if RESHADE_FX
	_effect_search_paths({ L".\\" }),
	_texture_search_paths({ L".\\" }),
#endif
	_config_path(g_reshade_base_path / L"ReShade.ini"),
	_screenshot_path(L".\\"),
	_screenshot_name("%AppName% %Date% %Time%"),
	_screenshot_post_save_command_arguments("\"%TargetPath%\""),
	_screenshot_post_save_command_working_directory(L".\\")
{
	assert(device != nullptr && graphics_queue != nullptr);

	_needs_update = check_for_update(_latest_version);

	// Default shortcut PrtScrn
	_screenshot_key_data[0] = 0x2C;

	// Increase global runtime index
	const unsigned int runtime_index = s_runtime_index++;

	// Fall back to alternative configuration file name if it exists
	std::error_code ec;
	if (std::filesystem::path config_path_alt = g_reshade_base_path / g_reshade_dll_path.filename().replace_extension(L".ini");
		std::filesystem::exists(config_path_alt, ec) && !std::filesystem::exists(_config_path, ec))
	{
		_config_path = std::move(config_path_alt);
	}
	// Add an index to the config file name in case there are multiple runtimes
	else if (runtime_index != 0)
	{
		const std::filesystem::path config_path_default = _config_path;
		_config_path.replace_filename(L"ReShade" + std::to_wstring(runtime_index + 1) + L".ini");
		if (std::filesystem::exists(config_path_default, ec) && !std::filesystem::exists(_config_path, ec))
			std::filesystem::copy_file(config_path_default, _config_path, ec);
	}

#if RESHADE_GUI
	init_gui();
#endif

	load_config();

	fpng::fpng_init();
}
reshade::runtime::~runtime()
{
	assert(_worker_threads.empty());
#if RESHADE_FX
	assert(!_is_initialized && _techniques.empty() && _technique_sorting.empty());
#endif

#if RESHADE_GUI
	// Save configuration before shutting down to ensure the current window state is written to disk
	save_config();
	ini_file::flush_cache(_config_path);

	deinit_gui();
#endif

	// Decrease global runtime index
	--s_runtime_index;
}

bool reshade::runtime::on_init(input::window_handle window)
{
	assert(!_is_initialized);

	const api::resource_desc back_buffer_desc = _device->get_resource_desc(get_back_buffer(0));

	_width = back_buffer_desc.texture.width;
	_height = back_buffer_desc.texture.height;
	_back_buffer_format = api::format_to_default_typed(back_buffer_desc.texture.format);
	_back_buffer_samples = back_buffer_desc.texture.samples;

	// Create resolve texture and copy pipeline (do this before creating effect resources, to ensure correct back buffer format is set up)
	if (back_buffer_desc.texture.samples > 1
		// Always use resolve texture in OpenGL to flip vertically and support sRGB + binding effect stencil
		|| _device->get_api() == api::device_api::opengl
#if RESHADE_FX
		// Some effects rely on there being an alpha channel available, so create resolve texture if that is not the case
		|| (_back_buffer_format == api::format::r8g8b8x8_unorm || _back_buffer_format == api::format::b8g8r8x8_unorm)
#endif
		)
	{
#if RESHADE_FX
		switch (_back_buffer_format)
		{
		case api::format::r8g8b8x8_unorm:
			_back_buffer_format = api::format::r8g8b8a8_unorm;
			break;
		case api::format::b8g8r8x8_unorm:
			_back_buffer_format = api::format::b8g8r8a8_unorm;
			break;
		}
#endif

		const bool need_copy_pipeline =
			_device->get_api() == api::device_api::d3d10 ||
			_device->get_api() == api::device_api::d3d11 ||
			_device->get_api() == api::device_api::d3d12;

		api::resource_usage usage = api::resource_usage::render_target | api::resource_usage::copy_dest | api::resource_usage::resolve_dest;
		if (need_copy_pipeline)
			usage |= api::resource_usage::shader_resource;
		else
			usage |= api::resource_usage::copy_source;

		if (!_device->create_resource(
				api::resource_desc(_width, _height, 1, 1, api::format_to_typeless(_back_buffer_format), 1, api::memory_heap::gpu_only, usage),
				nullptr, back_buffer_desc.texture.samples == 1 ? api::resource_usage::copy_dest : api::resource_usage::resolve_dest, &_back_buffer_resolved) ||
			!_device->create_resource_view(
				_back_buffer_resolved,
				api::resource_usage::render_target,
				api::resource_view_desc(api::format_to_default_typed(_back_buffer_format, 0)),
				&_back_buffer_targets.emplace_back()) ||
			!_device->create_resource_view(
				_back_buffer_resolved,
				api::resource_usage::render_target,
				api::resource_view_desc(api::format_to_default_typed(_back_buffer_format, 1)),
				&_back_buffer_targets.emplace_back()))
		{
			LOG(ERROR) << "Failed to create resolve texture resource!";
			goto exit_failure;
		}

		if (need_copy_pipeline)
		{
			if (!_device->create_resource_view(
					_back_buffer_resolved,
					api::resource_usage::shader_resource,
					api::resource_view_desc(_back_buffer_format),
					&_back_buffer_resolved_srv))
			{
				LOG(ERROR) << "Failed to create resolve shader resource view!";
				goto exit_failure;
			}

			api::sampler_desc sampler_desc = {};
			sampler_desc.filter = api::filter_mode::min_mag_mip_point;
			sampler_desc.address_u = api::texture_address_mode::clamp;
			sampler_desc.address_v = api::texture_address_mode::clamp;
			sampler_desc.address_w = api::texture_address_mode::clamp;

			api::pipeline_layout_param layout_params[2];
			layout_params[0] = api::descriptor_range { 0, 0, 0, 1, api::shader_stage::all, 1, api::descriptor_type::sampler };
			layout_params[1] = api::descriptor_range { 0, 0, 0, 1, api::shader_stage::all, 1, api::descriptor_type::shader_resource_view };

			const resources::data_resource vs = resources::load_data_resource(IDR_FULLSCREEN_VS);
			const resources::data_resource ps = resources::load_data_resource(IDR_COPY_PS);

			api::shader_desc vs_desc = { vs.data, vs.data_size };
			api::shader_desc ps_desc = { ps.data, ps.data_size };

			std::vector<api::pipeline_subobject> subobjects;
			subobjects.push_back({ api::pipeline_subobject_type::vertex_shader, 1, &vs_desc });
			subobjects.push_back({ api::pipeline_subobject_type::pixel_shader, 1, &ps_desc });

			if (!_device->create_pipeline_layout(2, layout_params, &_copy_pipeline_layout) ||
				!_device->create_pipeline(_copy_pipeline_layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &_copy_pipeline) ||
				!_device->create_sampler(sampler_desc, &_copy_sampler_state))
			{
				LOG(ERROR) << "Failed to create copy pipeline!";
				goto exit_failure;
			}
		}
	}

#if RESHADE_FX
	// Create an empty texture, which is bound to shader resource view slots with an unknown semantic (since it is not valid to bind a zero handle in Vulkan, unless the 'VK_EXT_robustness2' extension is enabled)
	if (_empty_tex == 0)
	{
		// Use VK_FORMAT_R16_SFLOAT format, since it is mandatory according to the spec (see https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#features-required-format-support)
		if (!_device->create_resource(
				api::resource_desc(1, 1, 1, 1, api::format::r16_float, 1, api::memory_heap::gpu_only, api::resource_usage::shader_resource),
				nullptr, api::resource_usage::shader_resource, &_empty_tex))
		{
			LOG(ERROR) << "Failed to create empty texture resource!";
			goto exit_failure;
		}

		_device->set_resource_name(_empty_tex, "ReShade empty texture");

		if (!_device->create_resource_view(_empty_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format::r16_float), &_empty_srv))
		{
			LOG(ERROR) << "Failed to create empty texture shader resource view!";
			goto exit_failure;
		}
	}

	// Create effect color and stencil resource
	if (_effect_stencil_format == api::format::unknown)
	{
		// Find a supported stencil format with the smallest footprint (since the depth component is not used)
		constexpr api::format possible_stencil_formats[] = {
			api::format::s8_uint,
			api::format::d16_unorm_s8_uint,
			api::format::d24_unorm_s8_uint,
			api::format::d32_float_s8_uint
		};

		for (const api::format format : possible_stencil_formats)
		{
			if (_device->check_format_support(format, api::resource_usage::depth_stencil))
			{
				_effect_stencil_format = format;
				break;
			}
		}
	}

	if (!update_effect_color_and_stencil_tex(_width, _height, _back_buffer_format, _effect_stencil_format))
		goto exit_failure;
#endif

	// Create render targets for the back buffer resources
	for (uint32_t i = 0; i < get_back_buffer_count(); ++i)
	{
		const api::resource back_buffer_resource = get_back_buffer(i);

		if (!_device->create_resource_view(
				back_buffer_resource,
				api::resource_usage::render_target,
				api::resource_view_desc(
					back_buffer_desc.texture.samples > 1 ? api::resource_view_type::texture_2d_multisample : api::resource_view_type::texture_2d,
					api::format_to_default_typed(back_buffer_desc.texture.format, 0), 0, 1, 0, 1),
				&_back_buffer_targets.emplace_back()) ||
			!_device->create_resource_view(
				back_buffer_resource,
				api::resource_usage::render_target,
				api::resource_view_desc(
					back_buffer_desc.texture.samples > 1 ? api::resource_view_type::texture_2d_multisample : api::resource_view_type::texture_2d,
					api::format_to_default_typed(back_buffer_desc.texture.format, 1), 0, 1, 0, 1),
				&_back_buffer_targets.emplace_back()))
		{
			LOG(ERROR) << "Failed to create back buffer render targets!";
			goto exit_failure;
		}
	}

#if RESHADE_GUI
	if (!init_imgui_resources())
		goto exit_failure;

	if (_is_vr && !init_gui_vr())
		goto exit_failure;
#endif

	if (window != nullptr && !_is_vr)
		_input = input::register_window(window);
	else
		_input.reset();

	// GTK 3 enables transparency for windows, which messes with effects that do not return an alpha value, so disable that again
	if (window != nullptr)
		utils::set_window_transparency(window, global_config().get("APP", "EnableTransparency"));

	// Reset frame count to zero so effects are loaded in 'update_effects'
	_frame_count = 0;

	_is_initialized = true;
	_last_reload_time = std::chrono::high_resolution_clock::time_point::max();

	_preset_save_successfull = true;
	_last_screenshot_save_successfull = true;

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_effect_runtime>(this);
#endif

	LOG(INFO) << "Recreated runtime environment on runtime " << this << " (" << _config_path << ").";

	return true;

exit_failure:
#if RESHADE_FX
	_device->destroy_resource(_empty_tex);
	_empty_tex = {};
	_device->destroy_resource_view(_empty_srv);
	_empty_srv = {};

	_device->destroy_resource(_effect_color_tex);
	_effect_color_tex = {};
	_device->destroy_resource_view(_effect_color_srv[0]);
	_effect_color_srv[0] = {};
	_device->destroy_resource_view(_effect_color_srv[1]);
	_effect_color_srv[1] = {};

	_device->destroy_resource(_effect_stencil_tex);
	_effect_stencil_tex = {};
	_device->destroy_resource_view(_effect_stencil_dsv);
	_effect_stencil_dsv = {};
#endif

	_device->destroy_pipeline(_copy_pipeline);
	_copy_pipeline = {};
	_device->destroy_pipeline_layout(_copy_pipeline_layout);
	_copy_pipeline_layout = {};
	_device->destroy_sampler(_copy_sampler_state);
	_copy_sampler_state = {};

	_device->destroy_resource(_back_buffer_resolved);
	_back_buffer_resolved = {};
	_device->destroy_resource_view(_back_buffer_resolved_srv);
	_back_buffer_resolved_srv = {};

	for (const api::resource_view view : _back_buffer_targets)
		_device->destroy_resource_view(view);
	_back_buffer_targets.clear();

#if RESHADE_GUI
	if (_is_vr)
		deinit_gui_vr();

	destroy_imgui_resources();
#endif

	return false;
}
void reshade::runtime::on_reset()
{
	if (_is_initialized)
		// Update initialization state immediately, so that any effect loading still in progress can abort early
		_is_initialized = false;
	else
		return; // Nothing to do if the runtime was already destroyed or not successfully initialized in the first place

#if RESHADE_FX
	// Already performs a wait for idle, so no need to do it again before destroying resources below
	destroy_effects();

	_device->destroy_resource(_empty_tex);
	_empty_tex = {};
	_device->destroy_resource_view(_empty_srv);
	_empty_srv = {};

	_device->destroy_resource(_effect_color_tex);
	_effect_color_tex = {};
	_device->destroy_resource_view(_effect_color_srv[0]);
	_effect_color_srv[0] = {};
	_device->destroy_resource_view(_effect_color_srv[1]);
	_effect_color_srv[1] = {};

	_device->destroy_resource(_effect_stencil_tex);
	_effect_stencil_tex = {};
	_device->destroy_resource_view(_effect_stencil_dsv);
	_effect_stencil_dsv = {};
#else
	for (std::thread &thread : _worker_threads)
		if (thread.joinable())
			thread.join();
	_worker_threads.clear();
#endif

	_device->destroy_pipeline(_copy_pipeline);
	_copy_pipeline = {};
	_device->destroy_pipeline_layout(_copy_pipeline_layout);
	_copy_pipeline_layout = {};
	_device->destroy_sampler(_copy_sampler_state);
	_copy_sampler_state = {};

	_device->destroy_resource(_back_buffer_resolved);
	_back_buffer_resolved = {};
	_device->destroy_resource_view(_back_buffer_resolved_srv);
	_back_buffer_resolved_srv = {};

	for (const api::resource_view view : _back_buffer_targets)
		_device->destroy_resource_view(view);
	_back_buffer_targets.clear();

	_width = _height = 0;

#if RESHADE_GUI
	if (_is_vr)
		deinit_gui_vr();

	destroy_imgui_resources();
#endif

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_effect_runtime>(this);
#endif

	LOG(INFO) << "Destroyed runtime environment on runtime " << this << " (" << _config_path << ").";
}
void reshade::runtime::on_present()
{
	assert(is_initialized());

#if RESHADE_ADDON
	_is_in_present_call = true;
#endif

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

	uint32_t back_buffer_index = get_current_back_buffer_index();
	const api::resource back_buffer_resource = get_back_buffer(back_buffer_index);

	// Resolve MSAA back buffer if MSAA is active or copy when format conversion is required
	if (_back_buffer_resolved != 0)
	{
		if (_back_buffer_samples == 1)
		{
			cmd_list->barrier(back_buffer_resource, api::resource_usage::present, api::resource_usage::copy_source);
			cmd_list->copy_texture_region(back_buffer_resource, 0, nullptr, _back_buffer_resolved, 0, nullptr);
			cmd_list->barrier(_back_buffer_resolved, api::resource_usage::copy_dest, api::resource_usage::render_target);
		}
		else
		{
			cmd_list->barrier(back_buffer_resource, api::resource_usage::present, api::resource_usage::resolve_source);
			cmd_list->resolve_texture_region(back_buffer_resource, 0, nullptr, _back_buffer_resolved, 0, 0, 0, 0, _back_buffer_format);
			cmd_list->barrier(_back_buffer_resolved, api::resource_usage::resolve_dest, api::resource_usage::render_target);
		}
	}

#if RESHADE_FX
	update_effects();

	if (_effects_enabled && !_effects_rendered_this_frame)
	{
		if (_should_save_screenshot && _screenshot_save_before)
			save_screenshot(" original");

		if (_back_buffer_resolved != 0)
		{
			runtime::render_effects(cmd_list, _back_buffer_targets[0], _back_buffer_targets[1]);
		}
		else
		{
			cmd_list->barrier(back_buffer_resource, api::resource_usage::present, api::resource_usage::render_target);
			runtime::render_effects(cmd_list, _back_buffer_targets[back_buffer_index * 2], _back_buffer_targets[back_buffer_index * 2 + 1]);
			cmd_list->barrier(back_buffer_resource, api::resource_usage::render_target, api::resource_usage::present);
		}
	}
#endif

	if (_should_save_screenshot)
		save_screenshot();

	_frame_count++;
	const auto current_time = std::chrono::high_resolution_clock::now();
	_last_frame_duration = current_time - _last_present_time; _last_present_time = current_time;

#ifdef NDEBUG
	// Lock input so it cannot be modified by other threads while we are reading it here
	const std::shared_lock<std::shared_mutex> input_lock = (_input != nullptr) ?
		_input->lock() : std::shared_lock<std::shared_mutex>();
#endif

#if RESHADE_GUI
	// Draw overlay
	if (_is_vr)
		draw_gui_vr();
	else
		draw_gui();

	if (_should_save_screenshot && _screenshot_save_gui && (_show_overlay
#if RESHADE_FX
		|| (_preview_texture != 0 && _effects_enabled)
#endif
		))
		save_screenshot(" overlay");
#endif

	// All screenshots were created at this point, so reset request
	_should_save_screenshot = false;

	// Handle keyboard shortcuts
	if (!_ignore_shortcuts && _input != nullptr)
	{
#if RESHADE_FX
		if (_input->is_key_pressed(_effects_key_data, _force_shortcut_modifiers))
			_effects_enabled = !_effects_enabled;
#endif

		if (_input->is_key_pressed(_screenshot_key_data, _force_shortcut_modifiers))
		{
			_screenshot_count++;
			_should_save_screenshot = true; // Remember that we want to save a screenshot next frame
		}

#if RESHADE_FX
		// Do not allow the following shortcuts while effects are being loaded or initialized (since they affect that state)
		if (!is_loading())
		{
			if (_input->is_key_pressed(_reload_key_data, _force_shortcut_modifiers))
				reload_effects();

			if (_input->is_key_pressed(_performance_mode_key_data, _force_shortcut_modifiers))
			{
				_performance_mode = !_performance_mode;
				save_config();
				reload_effects();
			}

			if (const bool reversed = _input->is_key_pressed(_prev_preset_key_data, _force_shortcut_modifiers);
				reversed || _input->is_key_pressed(_next_preset_key_data, _force_shortcut_modifiers))
			{
				// The preset shortcut key was pressed down, so start the transition
				if (switch_to_next_preset(_current_preset_path.parent_path(), reversed))
					save_config();
			}
			else
			{
				for (const preset_shortcut &shortcut : _preset_shortcuts)
				{
					if (_input->is_key_pressed(shortcut.key_data, _force_shortcut_modifiers))
					{
						if (switch_to_next_preset(shortcut.preset_path))
							save_config();
						break;
					}
				}
			}

			// Continuously update preset values while a transition is in progress
			if (_is_in_between_presets_transition)
				load_current_preset();
		}
#endif
	}

	// Stretch main render target back into MSAA back buffer if MSAA is active or copy when format conversion is required
	if (_back_buffer_resolved != 0)
	{
		const api::resource resources[2] = { back_buffer_resource, _back_buffer_resolved };
		const api::resource_usage state_old[2] = { api::resource_usage::copy_source | api::resource_usage::resolve_source, api::resource_usage::render_target };
		const api::resource_usage state_final[2] = { api::resource_usage::present, api::resource_usage::resolve_dest };

		if (_device->get_api() == api::device_api::d3d10 ||
			_device->get_api() == api::device_api::d3d11 ||
			_device->get_api() == api::device_api::d3d12)
		{
			const api::resource_usage state_new[2] = { api::resource_usage::render_target, api::resource_usage::shader_resource };

			cmd_list->barrier(2, resources, state_old, state_new);

			cmd_list->bind_pipeline(api::pipeline_stage::all_graphics, _copy_pipeline);

			cmd_list->push_descriptors(api::shader_stage::pixel, _copy_pipeline_layout, 0, api::descriptor_set_update { {}, 0, 0, 1, api::descriptor_type::sampler, &_copy_sampler_state });
			cmd_list->push_descriptors(api::shader_stage::pixel, _copy_pipeline_layout, 1, api::descriptor_set_update { {}, 0, 0, 1, api::descriptor_type::shader_resource_view, &_back_buffer_resolved_srv });

			const api::viewport viewport = { 0.0f, 0.0f, static_cast<float>(_width), static_cast<float>(_height), 0.0f, 1.0f };
			cmd_list->bind_viewports(0, 1, &viewport);
			const api::rect scissor_rect = { 0, 0, static_cast<int32_t>(_width), static_cast<int32_t>(_height) };
			cmd_list->bind_scissor_rects(0, 1, &scissor_rect);

			const bool srgb_write_enable = (_back_buffer_format == api::format::r8g8b8a8_unorm_srgb || _back_buffer_format == api::format::b8g8r8a8_unorm_srgb);
			cmd_list->bind_render_targets_and_depth_stencil(1, &_back_buffer_targets[2 + back_buffer_index * 2 + srgb_write_enable]);

			cmd_list->draw(3, 1, 0, 0);

			cmd_list->barrier(2, resources, state_new, state_final);
		}
		else
		{
			const api::resource_usage state_new[2] = { api::resource_usage::copy_dest, api::resource_usage::copy_source };

			cmd_list->barrier(2, resources, state_old, state_new);
			cmd_list->copy_texture_region(_back_buffer_resolved, 0, nullptr, back_buffer_resource, 0, nullptr);
			cmd_list->barrier(2, resources, state_new, state_final);
		}
	}

#if RESHADE_ADDON
	invoke_addon_event<addon_event::reshade_present>(this);

	_is_in_present_call = false;
#endif
#if RESHADE_FX
	_effects_rendered_this_frame = false;
#endif

	// Update input status
	if (_input != nullptr)
		_input->next_frame();
	if (_input_gamepad != nullptr)
		_input_gamepad->next_frame();

	// Save modified INI files
	if (!ini_file::flush_cache())
		_preset_save_successfull = false;

#if RESHADE_ADDON_LITE
	// Detect high network traffic
	extern volatile long g_network_traffic;

	static int cooldown = 0, traffic = 0;
	if (cooldown-- > 0)
	{
		traffic += g_network_traffic > 0;
	}
	else
	{
		const bool was_enabled = addon_enabled;
		addon_enabled = traffic < 10;
		traffic = 0;
		cooldown = 60;

#if RESHADE_FX
		if (addon_enabled != was_enabled)
		{
			if (was_enabled)
				_backup_texture_semantic_bindings = _texture_semantic_bindings;

			for (const auto &info : _backup_texture_semantic_bindings)
			{
				if (info.second.first == _effect_color_srv[0] && info.second.second == _effect_color_srv[1])
					continue;

				update_texture_bindings(info.first.c_str(), addon_enabled ? info.second.first : api::resource_view { 0 }, addon_enabled ? info.second.second : api::resource_view { 0 });
			}
		}
#endif
	}

	if (std::numeric_limits<long>::max() != g_network_traffic)
		g_network_traffic = 0;
#endif
}

void reshade::runtime::load_config()
{
	const ini_file &config = ini_file::load_cache(_config_path);

	if (config.get("INPUT", "GamepadNavigation"))
		_input_gamepad = input_gamepad::load();
	else
		_input_gamepad.reset();

	config.get("INPUT", "ForceShortcutModifiers", _force_shortcut_modifiers);
	config.get("INPUT", "KeyScreenshot", _screenshot_key_data);
#if RESHADE_FX
	config.get("INPUT", "KeyEffects", _effects_key_data);
	config.get("INPUT", "KeyNextPreset", _next_preset_key_data);
	config.get("INPUT", "KeyPerformanceMode", _performance_mode_key_data);
	config.get("INPUT", "KeyPreviousPreset", _prev_preset_key_data);
	config.get("INPUT", "KeyReload", _reload_key_data);

	config.get("GENERAL", "NoDebugInfo", _no_debug_info);
	config.get("GENERAL", "NoEffectCache", _no_effect_cache);
	config.get("GENERAL", "NoReloadOnInit", _no_reload_on_init);
	config.get("GENERAL", "NoReloadOnInitForNonVR", _no_reload_for_non_vr);

	config.get("GENERAL", "EffectSearchPaths", _effect_search_paths);
	config.get("GENERAL", "PerformanceMode", _performance_mode);
	config.get("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
	config.get("GENERAL", "SkipLoadingDisabledEffects", _effect_load_skipping);
	config.get("GENERAL", "TextureSearchPaths", _texture_search_paths);
	config.get("GENERAL", "IntermediateCachePath", _effect_cache_path);

	config.get("GENERAL", "StartupPresetPath", _startup_preset_path);
	config.get("GENERAL", "PresetPath", _current_preset_path);
	config.get("GENERAL", "PresetTransitionDuration", _preset_transition_duration);

	// Fall back to temp directory if cache path does not exist
	std::error_code ec;
	if (_effect_cache_path.empty() || !resolve_path(_effect_cache_path, ec))
	{
		_effect_cache_path = std::filesystem::temp_directory_path(ec) / "ReShade";
		std::filesystem::create_directory(_effect_cache_path, ec);
		if (ec)
			LOG(ERROR) << "Failed to create effect cache directory " << _effect_cache_path << " with error code " << ec.value() << '!';
	}

	// Use startup preset instead of last selection
	if (!_startup_preset_path.empty() && resolve_preset_path(_startup_preset_path, ec))
		_current_preset_path = _startup_preset_path;
	// Use default if the preset file does not exist yet
	else if (!resolve_preset_path(_current_preset_path, ec))
		_current_preset_path = g_reshade_base_path / L"ReShadePreset.ini";

	std::vector<unsigned int> preset_key_data;
	std::vector<std::filesystem::path> preset_shortcut_paths;
	config.get("GENERAL", "PresetShortcutKeys", preset_key_data);
	config.get("GENERAL", "PresetShortcutPaths", preset_shortcut_paths);

	_preset_shortcuts.clear();
	for (size_t i = 0; i < preset_shortcut_paths.size() && (i * 4 + 4) <= preset_key_data.size(); ++i)
	{
		preset_shortcut shortcut;
		shortcut.preset_path = preset_shortcut_paths[i];
		std::copy_n(&preset_key_data[i * 4], 4, shortcut.key_data);
		_preset_shortcuts.push_back(std::move(shortcut));
	}
#endif

	config.get("SCREENSHOT", "SavePath", _screenshot_path);
	config.get("SCREENSHOT", "SoundPath", _screenshot_sound_path);
	config.get("SCREENSHOT", "ClearAlpha", _screenshot_clear_alpha);
	config.get("SCREENSHOT", "FileFormat", _screenshot_format);
	config.get("SCREENSHOT", "FileNaming", _screenshot_name);
	config.get("SCREENSHOT", "JPEGQuality", _screenshot_jpeg_quality);
#if RESHADE_FX
	config.get("SCREENSHOT", "SaveBeforeShot", _screenshot_save_before);
	config.get("SCREENSHOT", "SavePresetFile", _screenshot_include_preset);
#endif
#if RESHADE_GUI
	config.get("SCREENSHOT", "SaveOverlayShot", _screenshot_save_gui);
#endif
	config.get("SCREENSHOT", "PostSaveCommand", _screenshot_post_save_command);
	config.get("SCREENSHOT", "PostSaveCommandArguments", _screenshot_post_save_command_arguments);
	config.get("SCREENSHOT", "PostSaveCommandWorkingDirectory", _screenshot_post_save_command_working_directory);
	config.get("SCREENSHOT", "PostSaveCommandNoWindow", _screenshot_post_save_command_no_window);

#if RESHADE_GUI
	load_config_gui(config);
#endif
}
void reshade::runtime::save_config() const
{
	ini_file &config = ini_file::load_cache(_config_path);

	config.set("INPUT", "ForceShortcutModifiers", _force_shortcut_modifiers);
	config.set("INPUT", "KeyScreenshot", _screenshot_key_data);
#if RESHADE_FX
	config.set("INPUT", "KeyEffects", _effects_key_data);
	config.set("INPUT", "KeyNextPreset", _next_preset_key_data);
	config.set("INPUT", "KeyPerformanceMode", _performance_mode_key_data);
	config.set("INPUT", "KeyPreviousPreset", _prev_preset_key_data);
	config.set("INPUT", "KeyReload", _reload_key_data);

	config.set("GENERAL", "NoDebugInfo", _no_debug_info);
	config.set("GENERAL", "NoEffectCache", _no_effect_cache);
	config.set("GENERAL", "NoReloadOnInit", _no_reload_on_init);
	config.set("GENERAL", "NoReloadOnInitForNonVR", _no_reload_for_non_vr);

	config.set("GENERAL", "EffectSearchPaths", _effect_search_paths);
	config.set("GENERAL", "PerformanceMode", _performance_mode);
	config.set("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
	config.set("GENERAL", "SkipLoadingDisabledEffects", _effect_load_skipping);
	config.set("GENERAL", "TextureSearchPaths", _texture_search_paths);
	config.set("GENERAL", "IntermediateCachePath", _effect_cache_path);

	// Use ReShade DLL directory as base for relative preset paths (see 'resolve_preset_path')
	std::filesystem::path startup_preset_path = _startup_preset_path.lexically_proximate(g_reshade_base_path);
	if (!startup_preset_path.empty())
	{
		if (startup_preset_path.native().rfind(L"..", 0) != std::wstring::npos)
			startup_preset_path = _startup_preset_path;
		if (startup_preset_path.is_relative())
			startup_preset_path = L"." / startup_preset_path;
	}
	config.set("GENERAL", "StartupPresetPath", startup_preset_path);

	std::filesystem::path current_preset_path = _current_preset_path.lexically_proximate(g_reshade_base_path);
	if (current_preset_path.native().rfind(L"..", 0) != std::wstring::npos)
		current_preset_path = _current_preset_path; // Do not use relative path if preset is in a parent directory
	if (current_preset_path.is_relative()) // Prefix preset path with dot character to better indicate it being a relative path
		current_preset_path = L"." / current_preset_path;
	config.set("GENERAL", "PresetPath", current_preset_path);

	config.set("GENERAL", "PresetTransitionDuration", _preset_transition_duration);

	std::vector<unsigned int> preset_key_data;
	std::vector<std::filesystem::path> preset_shortcut_paths;
	for (const preset_shortcut &shortcut : _preset_shortcuts)
	{
		if (shortcut.key_data[0] == 0)
			continue;

		preset_key_data.push_back(shortcut.key_data[0]);
		preset_key_data.push_back(shortcut.key_data[1]);
		preset_key_data.push_back(shortcut.key_data[2]);
		preset_key_data.push_back(shortcut.key_data[3]);
		preset_shortcut_paths.push_back(shortcut.preset_path);
	}
	config.set("GENERAL", "PresetShortcutKeys", preset_key_data);
	config.set("GENERAL", "PresetShortcutPaths", preset_shortcut_paths);
#endif

	config.set("SCREENSHOT", "SavePath", _screenshot_path);
	config.set("SCREENSHOT", "SoundPath", _screenshot_sound_path);
	config.set("SCREENSHOT", "ClearAlpha", _screenshot_clear_alpha);
	config.set("SCREENSHOT", "FileFormat", _screenshot_format);
	config.set("SCREENSHOT", "FileNaming", _screenshot_name);
	config.set("SCREENSHOT", "JPEGQuality", _screenshot_jpeg_quality);
#if RESHADE_FX
	config.set("SCREENSHOT", "SaveBeforeShot", _screenshot_save_before);
	config.set("SCREENSHOT", "SavePresetFile", _screenshot_include_preset);
#endif
#if RESHADE_GUI
	config.set("SCREENSHOT", "SaveOverlayShot", _screenshot_save_gui);
#endif
	config.set("SCREENSHOT", "PostSaveCommand", _screenshot_post_save_command);
	config.set("SCREENSHOT", "PostSaveCommandArguments", _screenshot_post_save_command_arguments);
	config.set("SCREENSHOT", "PostSaveCommandWorkingDirectory", _screenshot_post_save_command_working_directory);
	config.set("SCREENSHOT", "PostSaveCommandNoWindow", _screenshot_post_save_command_no_window);

#if RESHADE_GUI
	save_config_gui(config);
#endif
}

#if RESHADE_FX
void reshade::runtime::load_current_preset()
{
	_preset_save_successfull = true;

	const ini_file &preset = ini_file::load_cache(_current_preset_path);

	std::vector<std::string> technique_list;
	preset.get({}, "Techniques", technique_list);
	std::vector<std::string> sorted_technique_list;
	preset.get({}, "TechniqueSorting", sorted_technique_list);

	std::unordered_map<std::string, std::vector<std::pair<std::string, std::string>>> preset_preprocessor_definitions;
	preset.get({}, "PreprocessorDefinitions", preset_preprocessor_definitions[{}]);
	for (const effect &effect : _effects)
		preset.get(effect.source_file.filename().u8string(), "PreprocessorDefinitions", preset_preprocessor_definitions[effect.source_file.filename().u8string()]);

	// Recompile effects if preprocessor definitions have changed or running in performance mode (in which case all preset values are compile-time constants)
	if (_reload_remaining_effects != 0) // ... unless this is the 'load_current_preset' call in 'update_effects'
	{
		if (_performance_mode || preset_preprocessor_definitions != _preset_preprocessor_definitions)
		{
			_preset_preprocessor_definitions = std::move(preset_preprocessor_definitions);
			reload_effects();
			return; // Preset values are loaded in 'update_effects' during effect loading
		}

		if (std::find_if(technique_list.cbegin(), technique_list.cend(),
				[this](const std::string &technique_name) {
					const size_t at_pos = technique_name.find('@');
					if (at_pos == std::string::npos)
						return true;
					const auto it = std::find_if(_effects.cbegin(), _effects.cend(),
						[effect_name = std::filesystem::u8path(technique_name.substr(at_pos + 1))](const effect &effect) {
							return effect_name == effect.source_file.filename();
						});
					return it != _effects.cend() && it->skipped;
				}) != technique_list.cend())
		{
			reload_effects();
			return;
		}
	}

	if (sorted_technique_list.empty())
		ini_file::load_cache(_config_path).get("GENERAL", "TechniqueSorting", sorted_technique_list);
	if (sorted_technique_list.empty())
		sorted_technique_list = technique_list;

	// Reorder techniques
	std::stable_sort(_technique_sorting.begin(), _technique_sorting.end(),
		[this, &sorted_technique_list](size_t lhs_technique_index, size_t rhs_technique_index) {
			const technique &lhs = _techniques[lhs_technique_index];
			const technique &rhs = _techniques[rhs_technique_index];

			const std::string lhs_unique = lhs.name + '@' + _effects[lhs.effect_index].source_file.filename().u8string();
			auto lhs_it = std::find(sorted_technique_list.cbegin(), sorted_technique_list.cend(), lhs_unique);
			lhs_it = (lhs_it == sorted_technique_list.cend()) ? std::find(sorted_technique_list.cbegin(), sorted_technique_list.cend(), lhs.name) : lhs_it;

			const std::string rhs_unique = rhs.name + '@' + _effects[rhs.effect_index].source_file.filename().u8string();
			auto rhs_it = std::find(sorted_technique_list.cbegin(), sorted_technique_list.cend(), rhs_unique);
			rhs_it = (rhs_it == sorted_technique_list.cend()) ? std::find(sorted_technique_list.cbegin(), sorted_technique_list.cend(), rhs.name) : rhs_it;

			if (lhs_it < rhs_it)
				return true;
			if (lhs_it > rhs_it)
				return false;

			// Keep the declaration order within an effect file
			if (lhs.effect_index == rhs.effect_index)
				return false;

			// Sort the remaining techniques alphabetically using their label or name
			std::string lhs_label(lhs.annotation_as_string("ui_label"));
			if (lhs_label.empty())
				lhs_label = lhs.name;
			std::transform(lhs_label.begin(), lhs_label.end(), lhs_label.begin(),
				[](std::string::value_type c) {
					return static_cast<std::string::value_type>(std::toupper(c));
				});

			std::string rhs_label(rhs.annotation_as_string("ui_label"));
			if (rhs_label.empty())
				rhs_label = rhs.name;
			std::transform(rhs_label.begin(), rhs_label.end(), rhs_label.begin(),
				[](std::string::value_type c) {
					return static_cast<std::string::value_type>(std::toupper(c));
				});

			return lhs_label < rhs_label;
		});

	// Compute times since the transition has started and how much is left till it should end
	auto transition_time = std::chrono::duration_cast<std::chrono::microseconds>(_last_present_time - _last_preset_switching_time).count();
	auto transition_ms_left = _preset_transition_duration - transition_time / 1000;
	auto transition_ms_left_from_last_frame = transition_ms_left + std::chrono::duration_cast<std::chrono::microseconds>(_last_frame_duration).count() / 1000;

	if (_is_in_between_presets_transition && transition_ms_left <= 0)
		_is_in_between_presets_transition = false;

	for (effect &effect : _effects)
	{
		const std::string effect_name = effect.source_file.filename().u8string();

		for (uniform &variable : effect.uniforms)
		{
			if (variable.special != special_uniform::none)
				continue;

			if (variable.supports_toggle_key())
			{
				if (!preset.get(effect_name, "Key" + variable.name, variable.toggle_key_data))
					std::memset(variable.toggle_key_data, 0, sizeof(variable.toggle_key_data));
			}

			// Reset values to defaults before loading from a new preset
			if (!_is_in_between_presets_transition)
				reset_uniform_value(variable);

			reshadefx::constant values, values_old;

			switch (variable.type.base)
			{
			case reshadefx::type::t_int:
				get_uniform_value(variable, values.as_int, variable.type.components());
				preset.get(effect_name, variable.name, values.as_int);
				set_uniform_value(variable, values.as_int, variable.type.components());
				break;
			case reshadefx::type::t_bool:
			case reshadefx::type::t_uint:
				get_uniform_value(variable, values.as_uint, variable.type.components());
				preset.get(effect_name, variable.name, values.as_uint);
				set_uniform_value(variable, values.as_uint, variable.type.components());
				break;
			case reshadefx::type::t_float:
				get_uniform_value(variable, values.as_float, variable.type.components());
				values_old = values;
				preset.get(effect_name, variable.name, values.as_float);
				if (_is_in_between_presets_transition)
				{
					// Perform smooth transition on floating point values
					for (unsigned int i = 0; i < variable.type.components(); i++)
					{
						const float value_left = (values.as_float[i] - values_old.as_float[i]);
						values.as_float[i] -= (value_left / transition_ms_left_from_last_frame) * transition_ms_left;
					}
				}
				set_uniform_value(variable, values.as_float, variable.type.components());
				break;
			}
		}
	}

	for (technique &tech : _techniques)
	{
		const std::string unique_name = tech.name + '@' + _effects[tech.effect_index].source_file.filename().u8string();

		// Ignore preset if "enabled" annotation is set
		if (tech.annotation_as_int("enabled") ||
			std::find(technique_list.cbegin(), technique_list.cend(), unique_name) != technique_list.cend() ||
			std::find(technique_list.cbegin(), technique_list.cend(), tech.name) != technique_list.cend())
			enable_technique(tech);
		else
			disable_technique(tech);

		if (!preset.get({}, "Key" + unique_name, tech.toggle_key_data) &&
			!preset.get({}, "Key" + tech.name, tech.toggle_key_data))
		{
			tech.toggle_key_data[0] = tech.annotation_as_int("toggle");
			tech.toggle_key_data[1] = tech.annotation_as_int("togglectrl");
			tech.toggle_key_data[2] = tech.annotation_as_int("toggleshift");
			tech.toggle_key_data[3] = tech.annotation_as_int("togglealt");
		}
	}

	// Reverse queue so that effects are enabled in the order they are defined in the preset (since the queue is worked from back to front)
	std::reverse(_reload_create_queue.begin(), _reload_create_queue.end());
}
void reshade::runtime::save_current_preset() const
{
	ini_file &preset = ini_file::load_cache(_current_preset_path);

	// Build list of active techniques and effects
	std::set<size_t> effect_list;
	std::vector<std::string> technique_list;
	technique_list.reserve(_techniques.size());
	std::vector<std::string> sorted_technique_list;
	sorted_technique_list.reserve(_technique_sorting.size());

	for (size_t technique_index : _technique_sorting)
	{
		const technique &tech = _techniques[technique_index];

		const std::string unique_name = tech.name + '@' + _effects[tech.effect_index].source_file.filename().u8string();

		if (tech.enabled)
			technique_list.push_back(unique_name);
		if (tech.enabled || tech.toggle_key_data[0] != 0)
			effect_list.insert(tech.effect_index);

		// Keep track of the order of all techniques and not just the enabled ones
		sorted_technique_list.push_back(unique_name);

		if (tech.toggle_key_data[0] != 0)
			preset.set({}, "Key" + unique_name, tech.toggle_key_data);
		else if (tech.annotation_as_int("toggle") != 0)
			preset.set({}, "Key" + unique_name, 0); // Overwrite default toggle key to none
		else
			preset.remove_key({}, "Key" + unique_name);
	}

	if (preset.has({}, "TechniqueSorting") || !std::equal(technique_list.cbegin(), technique_list.cend(), sorted_technique_list.cbegin()))
		preset.set({}, "TechniqueSorting", std::move(sorted_technique_list));

	preset.set({}, "Techniques", std::move(technique_list));

	if (const auto preset_it = _preset_preprocessor_definitions.find({});
		preset_it != _preset_preprocessor_definitions.end() && !preset_it->second.empty())
		preset.set({}, "PreprocessorDefinitions", preset_it->second);
	else
		preset.remove_key({}, "PreprocessorDefinitions");

	// TODO: Do we want to save spec constants here too? The preset will be rather empty in performance mode otherwise.
	for (size_t effect_index = 0; effect_index < _effects.size(); ++effect_index)
	{
		if (effect_list.find(effect_index) == effect_list.end())
			continue;

		const effect &effect = _effects[effect_index];

		const std::string effect_name = effect.source_file.filename().u8string();

		if (const auto preset_it = _preset_preprocessor_definitions.find(effect_name);
			preset_it != _preset_preprocessor_definitions.end() && !preset_it->second.empty())
			preset.set(effect_name, "PreprocessorDefinitions", preset_it->second);
		else
			preset.remove_key(effect_name, "PreprocessorDefinitions");

		for (const uniform &variable : effect.uniforms)
		{
			if (variable.special != special_uniform::none)
				continue;

			if (variable.supports_toggle_key())
			{
				// Save the shortcut key into the preset files
				if (variable.toggle_key_data[0] != 0)
					preset.set(effect_name, "Key" + variable.name, variable.toggle_key_data);
				else
					preset.remove_key(effect_name, "Key" + variable.name);
			}

			const unsigned int components = variable.type.components();
			reshadefx::constant values;

			switch (variable.type.base)
			{
			case reshadefx::type::t_int:
				get_uniform_value(variable, values.as_int, components);
				preset.set(effect_name, variable.name, values.as_int, components);
				break;
			case reshadefx::type::t_bool:
			case reshadefx::type::t_uint:
				get_uniform_value(variable, values.as_uint, components);
				preset.set(effect_name, variable.name, values.as_uint, components);
				break;
			case reshadefx::type::t_float:
				get_uniform_value(variable, values.as_float, components);
				preset.set(effect_name, variable.name, values.as_float, components);
				break;
			}
		}
	}
}

bool reshade::runtime::switch_to_next_preset(std::filesystem::path filter_path, bool reversed)
{
	std::error_code ec; // This is here to ignore file system errors below
	std::filesystem::path filter_text;

	resolve_path(filter_path, ec);

	if (const std::filesystem::file_type file_type = std::filesystem::status(filter_path, ec).type();
		file_type != std::filesystem::file_type::directory)
	{
		if (file_type == std::filesystem::file_type::not_found)
		{
			filter_text = filter_path.filename();
			if (!filter_text.empty())
				filter_path = filter_path.parent_path();
		}
		else
		{
			_current_preset_path = filter_path;
			_last_preset_switching_time = _last_present_time;
			_is_in_between_presets_transition = true;

			return true;
		}
	}

	size_t current_preset_index = std::numeric_limits<size_t>::max();
	std::vector<std::filesystem::path> preset_paths;

	for (std::filesystem::path preset_path : std::filesystem::directory_iterator(filter_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		// Skip anything that is not a valid preset file
		if (!resolve_preset_path(preset_path, ec))
			continue;

		// Keep track of the index of the current preset in the list of found preset files that is being build
		if (std::filesystem::equivalent(preset_path, _current_preset_path, ec))
		{
			current_preset_index = preset_paths.size();
			preset_paths.push_back(std::move(preset_path));
			continue;
		}

		const std::wstring preset_name = preset_path.stem();
		// Only add those files that are matching the filter text
		if (filter_text.empty() ||
			std::search(preset_name.cbegin(), preset_name.cend(), filter_text.native().begin(), filter_text.native().end(),
				[](auto c1, auto c2) { return towlower(c1) == towlower(c2); }) != preset_name.cend())
			preset_paths.push_back(std::move(preset_path));
	}

	if (preset_paths.empty())
		return false; // No valid preset files were found, so nothing more to do

	if (current_preset_index == std::numeric_limits<size_t>::max())
	{
		// Current preset was not in the filter path, so just use the first or last file
		if (reversed)
			_current_preset_path = preset_paths.back();
		else
			_current_preset_path = preset_paths.front();
	}
	else
	{
		// Current preset was found in the filter path, so use the file before or after it
		if (auto it = std::next(preset_paths.begin(), current_preset_index); reversed)
			_current_preset_path = (it == preset_paths.begin()) ? preset_paths.back() : *(--it);
		else
			_current_preset_path = (it == std::prev(preset_paths.end())) ? preset_paths.front() : *(++it);
	}

	_last_preset_switching_time = _last_present_time;
	_is_in_between_presets_transition = true;

	return true;
}

bool reshade::runtime::load_effect(const std::filesystem::path &source_file, const ini_file &preset, size_t effect_index, bool preprocess_required)
{
	const std::chrono::high_resolution_clock::time_point time_load_started = std::chrono::high_resolution_clock::now();

	// Generate a unique string identifying this effect
	std::string attributes;
	attributes += "app=" + g_target_executable_path.stem().u8string() + ';';
	attributes += "width=" + std::to_string(_effect_width) + ';';
	attributes += "height=" + std::to_string(_effect_height) + ';';
	attributes += "color_space=" + std::to_string(static_cast<uint32_t>(_back_buffer_color_space)) + ';';
	attributes += "color_bit_depth=" + std::to_string(format_color_bit_depth(_effect_color_format)) + ';';
	attributes += "version=" + std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION) + ';';
	attributes += "performance_mode=" + std::string(_performance_mode ? "1" : "0") + ';';
	attributes += "vendor=" + std::to_string(_vendor_id) + ';';
	attributes += "device=" + std::to_string(_device_id) + ';';

	const std::string effect_name = source_file.filename().u8string();

	std::vector<std::pair<std::string, std::string>> preprocessor_definitions = _global_preprocessor_definitions;
	// Insert preset preprocessor definitions before global ones, so that if there are duplicates, the preset ones are used (since 'add_macro_definition' succeeds only for the first occurance)
	if (const auto preset_it = _preset_preprocessor_definitions.find({});
		preset_it != _preset_preprocessor_definitions.end())
		preprocessor_definitions.insert(preprocessor_definitions.begin(), preset_it->second.cbegin(), preset_it->second.cend());
	if (const auto preset_it = _preset_preprocessor_definitions.find(effect_name);
		preset_it != _preset_preprocessor_definitions.end())
		preprocessor_definitions.insert(preprocessor_definitions.begin(), preset_it->second.cbegin(), preset_it->second.cend());

	for (const std::pair<std::string, std::string> &definition : preprocessor_definitions)
		attributes += definition.first + '=' + definition.second + ';';

	std::error_code ec;
	std::set<std::filesystem::path> include_paths;
	if (source_file.is_absolute())
		include_paths.emplace(source_file.parent_path());
	for (std::filesystem::path include_path : _effect_search_paths)
	{
		const bool recursive_search = include_path.filename() == L"**";
		if (recursive_search)
			include_path.remove_filename();

		if (resolve_path(include_path, ec))
		{
			include_paths.emplace(include_path);

			if (recursive_search)
			{
				for (const std::filesystem::directory_entry &entry : std::filesystem::recursive_directory_iterator(include_path, std::filesystem::directory_options::skip_permission_denied, ec))
					if (entry.is_directory(ec))
						include_paths.emplace(entry);
			}
		}
	}

	attributes += effect_name;
	attributes += '?';
	attributes += std::to_string(std::filesystem::last_write_time(source_file, ec).time_since_epoch().count());
	attributes += ';';

	// The actual included files are not known at this point, so detect changes to any ".fxh" files in the search paths
	for (const std::filesystem::path &include_path : include_paths)
	{
		for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(include_path, std::filesystem::directory_options::skip_permission_denied, ec))
		{
			if (entry.path().extension() == L".fxh")
			{
				attributes += entry.path().filename().u8string();
				attributes += '?';
				attributes += std::to_string(entry.last_write_time(ec).time_since_epoch().count());
				attributes += ';';
			}
		}
	}

	effect &effect = _effects[effect_index];

	const size_t source_hash = std::hash<std::string>()(attributes);
	if (source_file != effect.source_file || source_hash != effect.source_hash)
	{
		// Source hash has changed, reset effect and load from scratch, rather than updating
		effect = {};
		effect.source_file = source_file;
		effect.source_hash = source_hash;
	}

	if (_effect_load_skipping && !_load_option_disable_skipping && !_worker_threads.empty()) // Only skip during 'load_effects'
	{
		if (std::vector<std::string> techniques;
			preset.get({}, "Techniques", techniques))
		{
			effect.skipped = std::find_if(techniques.cbegin(), techniques.cend(),
				[&effect_name](const std::string &technique) {
					const size_t at_pos = technique.find('@') + 1;
					return at_pos == 0 || technique.find(effect_name, at_pos) == at_pos;
				}) == techniques.cend();

			if (effect.skipped)
			{
				if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
					_reload_remaining_effects--;
				return false;
			}
		}
	}

	bool skip_optimization = false;
	std::string code_preamble;

	bool source_cached = false;
	std::string source;
	if (!effect.preprocessed && (preprocess_required || (source_cached = load_effect_cache(source_file.stem().u8string() + '-' + std::to_string(_renderer_id) + '-' + std::to_string(source_hash), "i", source)) == false))
	{
		reshadefx::preprocessor pp;
		pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", _performance_mode ? "1" : "0");
		pp.add_macro_definition("__VENDOR__", std::to_string(_vendor_id));
		pp.add_macro_definition("__DEVICE__", std::to_string(_device_id));
		pp.add_macro_definition("__RENDERER__", std::to_string(_renderer_id));
		pp.add_macro_definition("__APPLICATION__", std::to_string( // Truncate hash to 32-bit, since lexer currently only supports 32-bit numbers anyway
			std::hash<std::string>()(g_target_executable_path.stem().u8string()) & 0xFFFFFFFF));
		pp.add_macro_definition("BUFFER_WIDTH", std::to_string(_effect_width));
		pp.add_macro_definition("BUFFER_HEIGHT", std::to_string(_effect_height));
		pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
		pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");
		pp.add_macro_definition("BUFFER_COLOR_SPACE", std::to_string(static_cast<uint32_t>(_back_buffer_color_space)));
		pp.add_macro_definition("BUFFER_COLOR_BIT_DEPTH", std::to_string(format_color_bit_depth(_effect_color_format)));

		for (const std::pair<std::string, std::string> &definition : preprocessor_definitions)
		{
			if (definition.first.empty())
				continue; // Skip invalid definitions

			pp.add_macro_definition(definition.first, definition.second.empty() ? "1" : definition.second);
		}

		for (const std::filesystem::path &include_path : include_paths)
			pp.add_include_path(include_path);

		// Add some conversion macros for compatibility with older versions of ReShade
		pp.append_string(
			"#define tex2Doffset(s, coords, offset) tex2D(s, coords, offset)\n"
			"#define tex2Dlodoffset(s, coords, offset) tex2Dlod(s, coords, offset)\n"
			"#define tex2Dgather(s, t, c) tex2Dgather##c(s, t)\n"
			"#define tex2Dgatheroffset(s, t, o, c) tex2Dgather##c(s, t, o)\n"
			"#define tex2Dgather0 tex2DgatherR\n"
			"#define tex2Dgather1 tex2DgatherG\n"
			"#define tex2Dgather2 tex2DgatherB\n"
			"#define tex2Dgather3 tex2DgatherA\n");

		// Load and preprocess the source file
		effect.preprocessed = pp.append_file(source_file);

		// Append preprocessor errors to the error list
		effect.errors += pp.errors();

		if (effect.preprocessed)
		{
			source = pp.output();

			for (const std::pair<std::string, std::string> &pragma : pp.used_pragma_directives())
			{
				if (pragma.first == "reshade")
				{
					if (pragma.second == "skipoptimization" || pragma.second == "nooptimization")
						skip_optimization = true;
					continue;
				}

				const std::string pragma_directive = "#pragma " + pragma.first + ' ' + pragma.second + '\n';

				code_preamble += pragma_directive;
				source = "// " + pragma_directive + source;
			}

			// Keep track of used preprocessor definitions (so they can be displayed in the overlay)
			effect.definitions.clear();
			for (const std::pair<std::string, std::string> &definition : pp.used_macro_definitions())
			{
				if (definition.first.size() < 8 ||
					definition.first[0] == '_' ||
					definition.first.compare(0, 7, "BUFFER_") == 0 ||
					definition.first.compare(0, 8, "RESHADE_") == 0 ||
					definition.first.find("INCLUDE_") != std::string::npos)
					continue;

				effect.definitions.emplace_back(definition.first, trim(definition.second));

				// Write used preprocessor definitions to the cached source
				source = "// " + definition.first + '=' + definition.second + '\n' + source;
			}

			std::sort(effect.definitions.begin(), effect.definitions.end());

			// Do not cache if any special pragma directives were used, to ensure they are read again next time
			if (!skip_optimization)
				source_cached = save_effect_cache(source_file.stem().u8string() + '-' + std::to_string(_renderer_id) + '-' + std::to_string(source_hash), "i", source);
		}

		// Keep track of included files
		effect.included_files = pp.included_files();
		std::sort(effect.included_files.begin(), effect.included_files.end()); // Sort file names alphabetically
	}
	else
	{
		if (!source.empty())
		{
			// Read used preprocessor definitions and pragmas from the cached source
			for (size_t offset = 0, next; source.compare(offset, 3, "// ") == 0; offset = next + 1)
			{
				offset += 3;
				next = source.find('\n', offset);
				if (next == std::string::npos)
					break;

				if (source.compare(offset, 7, "#pragma") == 0)
				{
					code_preamble += source.substr(offset, (next + 1) - offset);
				}
				else if (const size_t equals_index = source.find('=', offset);
					equals_index != std::string::npos)
				{
					effect.definitions.emplace_back(
						source.substr(offset, equals_index - offset),
						source.substr(equals_index + 1, next - (equals_index + 1)));
				}
			}

			std::sort(effect.definitions.begin(), effect.definitions.end());
		}
	}

	if (!effect.compiled && !source.empty())
	{
		unsigned shader_model;
		if (_renderer_id == 0x9000)
			shader_model = 30; // D3D9
		else if (_renderer_id < 0xa100)
			shader_model = 40; // D3D10 (including feature level 9)
		else if (_renderer_id < 0xb000)
			shader_model = 41; // D3D10.1
		else if (_renderer_id < 0xc000)
			shader_model = 50; // D3D11
		else
			shader_model = 51; // D3D12

		std::unique_ptr<reshadefx::codegen> codegen;
		if ((_renderer_id & 0xF0000) == 0)
			codegen.reset(reshadefx::create_codegen_hlsl(shader_model, !_no_debug_info, _performance_mode));
		else if (_renderer_id < 0x20000)
			codegen.reset(reshadefx::create_codegen_glsl(false, !_no_debug_info, _performance_mode, false, true));
		else // Vulkan uses SPIR-V input
			codegen.reset(reshadefx::create_codegen_spirv(true, !_no_debug_info, _performance_mode, false, false));

		reshadefx::parser parser;

		// Compile the pre-processed source code (try the compile even if the preprocessor step failed to get additional error information)
		effect.compiled = parser.parse(std::move(source), codegen.get());

		// Append parser errors to the error list
		effect.errors  += parser.errors();

		// Write result to effect module
		codegen->write_result(effect.module);

		if (effect.compiled)
		{
			effect.uniforms.clear();

			// Create space for all variables (aligned to 16 bytes)
			effect.uniform_data_storage.resize((effect.module.total_uniform_size + 15) & ~15);

			for (uniform variable : effect.module.uniforms)
			{
				variable.effect_index = effect_index;

				const std::string_view special = variable.annotation_as_string("source");
				if (special.empty()) /* Ignore if annotation is missing */
					variable.special = special_uniform::none;
				else if (special == "frametime")
					variable.special = special_uniform::frame_time;
				else if (special == "framecount")
					variable.special = special_uniform::frame_count;
				else if (special == "random")
					variable.special = special_uniform::random;
				else if (special == "pingpong")
					variable.special = special_uniform::ping_pong;
				else if (special == "date")
					variable.special = special_uniform::date;
				else if (special == "timer")
					variable.special = special_uniform::timer;
				else if (special == "key")
					variable.special = special_uniform::key;
				else if (special == "mousepoint")
					variable.special = special_uniform::mouse_point;
				else if (special == "mousedelta")
					variable.special = special_uniform::mouse_delta;
				else if (special == "mousebutton")
					variable.special = special_uniform::mouse_button;
				else if (special == "mousewheel")
					variable.special = special_uniform::mouse_wheel;
				else if (special == "ui_open" || special == "overlay_open")
					variable.special = special_uniform::overlay_open;
				else if (special == "ui_active" || special == "overlay_active")
					variable.special = special_uniform::overlay_active;
				else if (special == "ui_hovered" || special == "overlay_hovered")
					variable.special = special_uniform::overlay_hovered;
				else if (special == "screenshot")
					variable.special = special_uniform::screenshot;
				else
					variable.special = special_uniform::unknown;

				// Copy initial data into uniform storage area
				reset_uniform_value(variable);

				effect.uniforms.push_back(std::move(variable));
			}

			// Fill all specialization constants with values from the current preset
			if (_performance_mode)
			{
				for (reshadefx::uniform_info &constant : effect.module.spec_constants)
				{
					switch (constant.type.base)
					{
					case reshadefx::type::t_int:
						preset.get(effect_name, constant.name, constant.initializer_value.as_int);
						break;
					case reshadefx::type::t_bool:
					case reshadefx::type::t_uint:
						preset.get(effect_name, constant.name, constant.initializer_value.as_uint);
						break;
					case reshadefx::type::t_float:
						preset.get(effect_name, constant.name, constant.initializer_value.as_float);
						break;
					}

					// Check if this is a split specialization constant and move data accordingly
					if (constant.type.is_scalar() && constant.offset != 0)
						constant.initializer_value.as_uint[0] = constant.initializer_value.as_uint[constant.offset];

					if (_renderer_id >= 0x20000)
						continue;

					code_preamble += "#define SPEC_CONSTANT_" + constant.name + ' ';

					for (unsigned int i = 0; i < constant.type.components(); ++i)
					{
						switch (constant.type.base)
						{
						case reshadefx::type::t_bool:
							code_preamble += constant.initializer_value.as_uint[i] ? "true" : "false";
							break;
						case reshadefx::type::t_int:
							code_preamble += std::to_string(constant.initializer_value.as_int[i]);
							break;
						case reshadefx::type::t_uint:
							code_preamble += std::to_string(constant.initializer_value.as_uint[i]);
							break;
						case reshadefx::type::t_float:
							code_preamble += std::to_string(constant.initializer_value.as_float[i]);
							break;
						}

						if (i + 1 < constant.type.components())
							code_preamble += ", ";
					}

					code_preamble += '\n';
				}
			}
		}
	}

	if ( effect.compiled && (effect.preprocessed || source_cached))
	{
		// Compile shader modules
		for (const reshadefx::entry_point &entry_point : effect.module.entry_points)
		{
			if (entry_point.type == reshadefx::shader_type::cs && !_device->check_capability(api::device_caps::compute_shader))
			{
				effect.errors += "error: " + entry_point.name + ": compute shaders are not supported in D3D9/D3D10\n";
				effect.compiled = false;
				break;
			}

			std::string &cso = effect.assembly[entry_point.name];
			std::string &cso_text = effect.assembly_text[entry_point.name];

			if ((_renderer_id & 0xF0000) == 0)
			{
				assert(_d3d_compiler_module != nullptr);

				// Copy string, since this has to be repeated for every entry point
				std::string hlsl = code_preamble;

				if (_renderer_id == 0x9000)
				{
					// Create SEMANTIC_PIXEL_SIZE constants
					hlsl += "#define COLOR_PIXEL_SIZE 1.0 / " + std::to_string(_effect_width) + ", 1.0 / " + std::to_string(_effect_height) + '\n';

					uint32_t semantic_index = 0;
					for (const reshadefx::texture_info &tex : effect.module.textures)
					{
						if (tex.semantic.empty() || tex.semantic == "COLOR")
							continue;

						semantic_index++;
						assert((effect.uniform_data_storage.size() / 16) <= (255 - semantic_index));

						// Avoid duplicate declarations if the semantic was used multiple times
						if (hlsl.find(tex.semantic + "_PIXEL_SIZE") == std::string::npos)
							hlsl += "uniform float2 " + tex.semantic + "_PIXEL_SIZE : register(c" + std::to_string(255 - semantic_index) + ");\n";
					}
				}

				hlsl += "#line 1\n"; // Reset line number, so it matches what is shown when viewing the generated code
				hlsl.append(effect.module.code.data(), effect.module.code.size());

				// Overwrite position semantic in pixel shaders
				const D3D_SHADER_MACRO ps_defines[] = {
					{ "POSITION", "VPOS" }, { nullptr, nullptr }
				};

				std::string profile;
				switch (entry_point.type)
				{
				case reshadefx::shader_type::vs:
					profile = "vs";
					break;
				case reshadefx::shader_type::ps:
					profile = "ps";
					break;
				case reshadefx::shader_type::cs:
					profile = "cs";
					break;
				}

				switch (_renderer_id)
				{
				default:
				case D3D_FEATURE_LEVEL_11_0:
					profile += "_5_0";
					break;
				case D3D_FEATURE_LEVEL_10_1:
					profile += "_4_1";
					break;
				case D3D_FEATURE_LEVEL_10_0:
					profile += "_4_0";
					break;
				case D3D_FEATURE_LEVEL_9_1:
				case D3D_FEATURE_LEVEL_9_2:
					profile += "_4_0_level_9_1";
					break;
				case D3D_FEATURE_LEVEL_9_3:
					profile += "_4_0_level_9_3";
					break;
				case 0x9000:
					profile += "_3_0";
					break;
				}

				UINT compile_flags = 0;
				if (skip_optimization)
					compile_flags |= D3DCOMPILE_SKIP_OPTIMIZATION;
				else if (_performance_mode)
					compile_flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
				if (_renderer_id >= D3D_FEATURE_LEVEL_10_0)
					compile_flags |= D3DCOMPILE_ENABLE_STRICTNESS;
#ifndef NDEBUG
				compile_flags |= D3DCOMPILE_DEBUG;
#endif

				std::string hlsl_attributes;
				hlsl_attributes += "entrypoint=" + entry_point.name + ';';
				hlsl_attributes += "profile=" + profile + ';';
				hlsl_attributes += "flags=" + std::to_string(compile_flags) + ';';

				const std::string cache_id =
					effect.source_file.stem().u8string() + '-' + entry_point.name + '-' + std::to_string(_renderer_id) + '-' +
					std::to_string(std::hash<std::string_view>()(hlsl_attributes) ^ std::hash<std::string_view>()(hlsl));

				if (!load_effect_cache(cache_id, "cso", cso))
				{
					const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(static_cast<HMODULE>(_d3d_compiler_module), "D3DCompile"));
					assert(D3DCompile != nullptr);

					com_ptr<ID3DBlob> d3d_compiled, d3d_errors;
					const HRESULT hr = D3DCompile(
						hlsl.data(), hlsl.size(),
						nullptr, entry_point.type == reshadefx::shader_type::ps ? ps_defines : nullptr, nullptr,
						entry_point.name.c_str(),
						profile.c_str(),
						compile_flags, 0,
						&d3d_compiled, &d3d_errors);

					std::string d3d_errors_string;
					if (d3d_errors != nullptr) // Append warnings to the output error string as well
						d3d_errors_string.assign(static_cast<const char *>(d3d_errors->GetBufferPointer()), d3d_errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

					// De-duplicate error lines (D3DCompiler sometimes repeats the same error multiple times)
					for (size_t line_offset = 0, next_line_offset; (next_line_offset = d3d_errors_string.find('\n', line_offset)) != std::string::npos; line_offset = next_line_offset + 1)
					{
						const std::string_view cur_line(d3d_errors_string.data() + line_offset, next_line_offset - line_offset);

						if (const size_t end_offset = d3d_errors_string.find('\n', next_line_offset + 1);
							end_offset != std::string::npos)
						{
							const std::string_view next_line(d3d_errors_string.data() + next_line_offset + 1, end_offset - next_line_offset - 1);
							if (cur_line == next_line)
							{
								d3d_errors_string.erase(next_line_offset, end_offset - next_line_offset);
								next_line_offset = line_offset - 1;
							}
						}

						// Also remove D3DCompiler warnings about 'groupshared' specifier used in VS/PS modules
						if (cur_line.find("X3579") != std::string_view::npos)
						{
							d3d_errors_string.erase(line_offset, next_line_offset + 1 - line_offset);
							next_line_offset = line_offset - 1;
						}
					}

					if (FAILED(hr))
					{
						// Add a prefix with the offending entry point name for generic error messages like an out of memory notification
						if (d3d_errors_string.find("error") == std::string::npos)
							effect.errors += "error: " + entry_point.name + ": ";

						effect.errors += d3d_errors_string;
						effect.compiled = false;

						d3d_errors.reset();
						d3d_compiled.reset();
						break;
					}
					else
					{
						// Append warnings
						effect.errors += d3d_errors_string;
					}

					cso.resize(d3d_compiled->GetBufferSize());
					std::memcpy(cso.data(), d3d_compiled->GetBufferPointer(), cso.size());

					save_effect_cache(cache_id, "cso", cso);
				}

				if (!load_effect_cache(cache_id, "asm", cso_text))
				{
					const auto D3DDisassemble = reinterpret_cast<pD3DDisassemble>(GetProcAddress(static_cast<HMODULE>(_d3d_compiler_module), "D3DDisassemble"));
					assert(D3DDisassemble != nullptr);

					com_ptr<ID3DBlob> d3d_disassembled;
					if (SUCCEEDED(D3DDisassemble(cso.data(), cso.size(), 0, nullptr, &d3d_disassembled)))
						cso_text.assign(static_cast<const char *>(d3d_disassembled->GetBufferPointer()), d3d_disassembled->GetBufferSize() - 1);

					save_effect_cache(cache_id, "asm", cso_text);
				}
			}
			else if (_renderer_id < 0x20000)
			{
				std::string glsl = "#version 430\n#define ENTRY_POINT_" + entry_point.name + " 1\n";

				if (entry_point.type != reshadefx::shader_type::ps)
				{
					// OpenGL does not allow using 'discard' in the vertex shader profile
					glsl += "#define discard\n";
					// 'dFdx', 'dFdx' and 'fwidth' too are only available in fragment shaders
					glsl += "#define dFdx(x) x\n";
					glsl += "#define dFdy(y) y\n";
					glsl += "#define fwidth(p) p\n";
				}
				if (entry_point.type != reshadefx::shader_type::cs)
				{
					// OpenGL does not allow using 'shared' in vertex/fragment shader profile
					glsl += "#define shared\n";
					glsl += "#define atomicAdd(a, b) a\n";
					glsl += "#define atomicAnd(a, b) a\n";
					glsl += "#define atomicOr(a, b) a\n";
					glsl += "#define atomicXor(a, b) a\n";
					glsl += "#define atomicMin(a, b) a\n";
					glsl += "#define atomicMax(a, b) a\n";
					glsl += "#define atomicExchange(a, b) a\n";
					glsl += "#define atomicCompSwap(a, b, c) a\n";
					// Barrier intrinsics are only available in compute shaders
					glsl += "#define barrier()\n";
					glsl += "#define memoryBarrier()\n";
					glsl += "#define groupMemoryBarrier()\n";
				}

				glsl += code_preamble;
				glsl += "#line 1 0\n"; // Reset line number, so it matches what is shown when viewing the generated code
				glsl.append(effect.module.code.data(), effect.module.code.size());

				cso_text = cso = std::move(glsl);
			}
			else
			{
				assert(_renderer_id >= 0x14600); // Core since OpenGL 4.6 (see https://www.khronos.org/opengl/wiki/SPIR-V)

				// There are various issues with SPIR-V modules that have multiple entry points on all major GPU vendors.
				// On AMD for instance creating a graphics pipeline just fails with a generic 'VK_ERROR_OUT_OF_HOST_MEMORY'. On NVIDIA artifacts occur on some driver versions.
				// To work around these problems, create a separate shader module for every entry point and rewrite the SPIR-V module for each to remove all but a single entry point (and associated functions/variables).
				uint32_t current_function = 0, current_function_offset = 0;
				// Copy SPIR-V, so that all but the current entry point are only removed from that copy
				std::vector<uint32_t> spirv(reinterpret_cast<const uint32_t *>(effect.module.code.data()), reinterpret_cast<const uint32_t *>(effect.module.code.data() + effect.module.code.size()));
				std::vector<uint32_t> functions_to_remove, variables_to_remove;

				for (uint32_t inst = 5 /* Skip SPIR-V header information */; inst < spirv.size();)
				{
					const uint32_t op = spirv[inst] & 0xFFFF;
					const uint32_t len = (spirv[inst] >> 16) & 0xFFFF;
					assert(len != 0);

					switch (op)
					{
					case 15 /* OpEntryPoint */:
						// Look for any non-matching entry points
						if (entry_point.name != reinterpret_cast<const char *>(&spirv[inst + 3]))
						{
							functions_to_remove.push_back(spirv[inst + 2]);

							// Get interface variables
							for (uint32_t k = inst + 3 + static_cast<uint32_t>((std::strlen(reinterpret_cast<const char *>(&spirv[inst + 3])) + 4) / 4); k < inst + len; ++k)
								variables_to_remove.push_back(spirv[k]);

							// Remove this entry point from the module
							spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
							continue;
						}
						break;
					case 16 /* OpExecutionMode */:
						if (std::find(functions_to_remove.begin(), functions_to_remove.end(), spirv[inst + 1]) != functions_to_remove.end())
						{
							spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
							continue;
						}
						break;
					case 59 /* OpVariable */:
						// Remove all declarations of the interface variables for non-matching entry points
						if (std::find(variables_to_remove.begin(), variables_to_remove.end(), spirv[inst + 2]) != variables_to_remove.end())
						{
							spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
							continue;
						}
						break;
					case 71 /* OpDecorate */:
						// Remove all decorations targeting any of the interface variables for non-matching entry points
						if (std::find(variables_to_remove.begin(), variables_to_remove.end(), spirv[inst + 1]) != variables_to_remove.end())
						{
							spirv.erase(spirv.begin() + inst, spirv.begin() + inst + len);
							continue;
						}
						break;
					case 54 /* OpFunction */:
						current_function = spirv[inst + 2];
						current_function_offset = inst;
						break;
					case 56 /* OpFunctionEnd */:
						// Remove all function definitions for non-matching entry points
						if (std::find(functions_to_remove.begin(), functions_to_remove.end(), current_function) != functions_to_remove.end())
						{
							spirv.erase(spirv.begin() + current_function_offset, spirv.begin() + inst + len);
							inst = current_function_offset;
							continue;
						}
						break;
					}

					inst += len;
				}

				cso.resize(spirv.size() * sizeof(uint32_t));
				std::memcpy(cso.data(), spirv.data(), cso.size());
			}
		}

		const std::unique_lock<std::shared_mutex> lock(_reload_mutex);

		for (texture new_texture : effect.module.textures)
		{
			new_texture.effect_index = effect_index;

			if (!new_texture.semantic.empty() && (new_texture.render_target || new_texture.storage_access))
			{
				effect.errors += "error: " + new_texture.unique_name + ": texture with a semantic used as a render target or storage\n";
				effect.compiled = false;
				break;
			}

			// Try to share textures with the same name across effects
			if (const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
					[&new_texture](const texture &item) {
						return item.unique_name == new_texture.unique_name;
					});
				existing_texture != _textures.end())
			{
				// Cannot share texture if this is a normal one, but the existing one is a reference and vice versa
				if (new_texture.semantic != existing_texture->semantic)
				{
					effect.errors += "error: " + new_texture.unique_name + ": another effect (";
					effect.errors += _effects[existing_texture->effect_index].source_file.filename().u8string();
					effect.errors += ") already created a texture with the same name but different semantic\n";
					effect.compiled = false;
					break;
				}

				if (new_texture.semantic.empty() && !existing_texture->matches_description(new_texture))
				{
					effect.errors += "warning: " + new_texture.unique_name + ": another effect (";
					effect.errors += _effects[existing_texture->effect_index].source_file.filename().u8string();
					effect.errors += ") already created a texture with the same name but different dimensions\n";
				}
				if (new_texture.semantic.empty() && (existing_texture->annotation_as_string("source") != new_texture.annotation_as_string("source")))
				{
					effect.errors += "warning: " + new_texture.unique_name + ": another effect (";
					effect.errors += _effects[existing_texture->effect_index].source_file.filename().u8string();
					effect.errors += ") already created a texture with a different image file\n";
				}

				if (existing_texture->semantic == "COLOR" && format_color_bit_depth(_effect_color_format) != 8)
				{
					for (const reshadefx::sampler_info &sampler_info : effect.module.samplers)
					{
						if (sampler_info.srgb && sampler_info.texture_name == new_texture.unique_name)
						{
							effect.errors += "warning: " + sampler_info.unique_name + ": texture does not support sRGB sampling (back buffer format is not RGBA8)\n";
						}
					}
				}

				if (std::find(existing_texture->shared.begin(), existing_texture->shared.end(), effect_index) == existing_texture->shared.end())
					existing_texture->shared.push_back(effect_index);

				// Always make shared textures render targets, since they may be used as such in a different effect
				existing_texture->render_target = true;
				existing_texture->storage_access = true;
				continue;
			}

			if (new_texture.annotation_as_int("pooled") && new_texture.semantic.empty())
			{
				// Try to find another pooled texture to share with (and do not share within the same effect)
				if (const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
						[&new_texture](const texture &item) {
							return item.annotation_as_int("pooled") && item.effect_index != new_texture.effect_index && item.matches_description(new_texture);
						});
					existing_texture != _textures.end())
				{
					// Overwrite referenced texture in samplers with the pooled one
					for (reshadefx::sampler_info &sampler_info : effect.module.samplers)
						if (sampler_info.texture_name == new_texture.unique_name)
							sampler_info.texture_name = existing_texture->unique_name;
					// Overwrite referenced texture in storages with the pooled one
					for (reshadefx::storage_info &storage_info : effect.module.storages)
						if (storage_info.texture_name == new_texture.unique_name)
							storage_info.texture_name = existing_texture->unique_name;
					// Overwrite referenced texture in render targets with the pooled one
					for (reshadefx::technique_info &technique_info : effect.module.techniques)
					{
						for (reshadefx::pass_info &pass_info : technique_info.passes)
						{
							std::replace(std::begin(pass_info.render_target_names), std::end(pass_info.render_target_names),
								new_texture.unique_name, existing_texture->unique_name);

							for (reshadefx::sampler_info &sampler_info : pass_info.samplers)
								if (sampler_info.texture_name == new_texture.unique_name)
									sampler_info.texture_name = existing_texture->unique_name;
							for (reshadefx::storage_info &storage_info : pass_info.storages)
								if (storage_info.texture_name == new_texture.unique_name)
									storage_info.texture_name = existing_texture->unique_name;
						}
					}

					if (std::find(existing_texture->shared.cbegin(), existing_texture->shared.cend(), effect_index) == existing_texture->shared.cend())
						existing_texture->shared.push_back(effect_index);

					existing_texture->render_target = true;
					existing_texture->storage_access = true;
					continue;
				}
			}

			// This is the first effect using this texture
			new_texture.shared.push_back(effect_index);

			_textures.push_back(std::move(new_texture));
		}

		for (technique new_technique : effect.module.techniques)
		{
			new_technique.effect_index = effect_index;

			new_technique.hidden = new_technique.annotation_as_int("hidden") != 0;
			new_technique.enabled_in_screenshot = new_technique.annotation_as_int("enabled_in_screenshot", 0, true) != 0;

			if (new_technique.annotation_as_int("enabled"))
				enable_technique(new_technique);

			_techniques.push_back(std::move(new_technique));
			_technique_sorting.push_back(_techniques.size() - 1);
		}
	}

	const std::chrono::high_resolution_clock::time_point time_load_finished = std::chrono::high_resolution_clock::now();

	if (_reload_remaining_effects != 0 && _reload_remaining_effects != std::numeric_limits<size_t>::max())
		_reload_remaining_effects--;
	else
		_reload_remaining_effects = 0; // Force effect initialization in 'update_effects'

	if ( effect.compiled && (effect.preprocessed || source_cached))
	{
		if (effect.errors.empty())
			LOG(INFO) << "Successfully compiled " << source_file << " in " << (std::chrono::duration_cast<std::chrono::milliseconds>(time_load_finished - time_load_started).count() * 1e-3f) << " s.";
		else
			LOG(WARN) << "Successfully compiled " << source_file << " in " << (std::chrono::duration_cast<std::chrono::milliseconds>(time_load_finished - time_load_started).count() * 1e-3f) << " s with warnings:\n" << effect.errors;
		return true;
	}
	else
	{
		_last_reload_successfull = false;

		if (effect.errors.empty())
			LOG(ERROR) << "Failed to compile " << source_file << '!';
		else
			LOG(ERROR) << "Failed to compile " << source_file << ":\n" << effect.errors;
		return false;
	}
}
bool reshade::runtime::create_effect(size_t effect_index)
{
	assert(effect_index < _effects.size());

	effect &effect = _effects[effect_index];

	// Create textures now, since they are referenced when building samplers below
	for (texture &tex : _textures)
	{
		if (tex.resource != 0 || std::find(tex.shared.cbegin(), tex.shared.cend(), effect_index) == tex.shared.cend())
			continue;

		if (!create_texture(tex))
		{
			effect.errors += "Failed to create texture " + tex.unique_name + '.';
			effect.compiled = false;
			_last_reload_successfull = false;
			return false;
		}
	}

	// Build specialization constants
	std::vector<uint32_t> spec_data;
	std::vector<uint32_t> spec_constants;
	for (const reshadefx::uniform_info &constant : effect.module.spec_constants)
	{
		uint32_t id = static_cast<uint32_t>(spec_constants.size());
		spec_data.push_back(constant.initializer_value.as_uint[0]);
		spec_constants.push_back(id);
	}

	// Create optional query pool for time measurements
	if (!_device->create_query_pool(api::query_type::timestamp, static_cast<uint32_t>(effect.module.techniques.size() * 2 * 4), &effect.query_pool))
		LOG(ERROR) << "Failed to create query pool for effect file " << effect.source_file << '!';

	const bool sampler_with_resource_view = _device->check_capability(api::device_caps::sampler_with_resource_view);

	api::descriptor_range layout_ranges[4];
	layout_ranges[0].binding = 0;
	layout_ranges[0].dx_register_index = 0; // b0 (global constant buffer)
	layout_ranges[0].dx_register_space = 0;
	layout_ranges[0].count = 1;
	layout_ranges[0].array_size = 1;
	layout_ranges[0].type = api::descriptor_type::constant_buffer;
	layout_ranges[0].visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	layout_ranges[1].binding = 0;
	layout_ranges[1].dx_register_index = 0; // s#
	layout_ranges[1].dx_register_space = 0;
	layout_ranges[1].count = effect.module.num_sampler_bindings;
	layout_ranges[1].array_size = 1;
	layout_ranges[1].type = sampler_with_resource_view ? api::descriptor_type::sampler_with_resource_view : api::descriptor_type::sampler;
	layout_ranges[1].visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	layout_ranges[2].binding = 0;
	layout_ranges[2].dx_register_index = 0; // t#
	layout_ranges[2].dx_register_space = 0;
	layout_ranges[2].count = effect.module.num_texture_bindings;
	layout_ranges[2].array_size = 1;
	layout_ranges[2].type = api::descriptor_type::shader_resource_view;
	layout_ranges[2].visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	layout_ranges[3].binding = 0;
	layout_ranges[3].dx_register_index = 0; // u#
	layout_ranges[3].dx_register_space = 0;
	layout_ranges[3].count = effect.module.num_storage_bindings;
	layout_ranges[3].array_size = 1;
	layout_ranges[3].type = api::descriptor_type::unordered_access_view;
	layout_ranges[3].visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	api::pipeline_layout_param layout_params[4];
	layout_params[0].type = api::pipeline_layout_param_type::descriptor_set;
	layout_params[0].descriptor_set.count = 1;
	layout_params[0].descriptor_set.ranges = &layout_ranges[0];

	layout_params[1].type = api::pipeline_layout_param_type::descriptor_set;
	layout_params[1].descriptor_set.count = 1;
	layout_params[1].descriptor_set.ranges = &layout_ranges[1];

	if (sampler_with_resource_view)
	{
		layout_params[2].type = api::pipeline_layout_param_type::descriptor_set;
		layout_params[2].descriptor_set.count = 1;
		layout_params[2].descriptor_set.ranges = &layout_ranges[3];
	}
	else
	{
		layout_params[2].type = api::pipeline_layout_param_type::descriptor_set;
		layout_params[2].descriptor_set.count = 1;
		layout_params[2].descriptor_set.ranges = &layout_ranges[2];
		layout_params[3].type = api::pipeline_layout_param_type::descriptor_set;
		layout_params[3].descriptor_set.count = 1;
		layout_params[3].descriptor_set.ranges = &layout_ranges[3];
	}

	// Create pipeline layout for this effect
	if (!_device->create_pipeline_layout(sampler_with_resource_view ? 3 : 4, layout_params, &effect.layout))
	{
		effect.compiled = false;
		_last_reload_successfull = false;

		LOG(ERROR) << "Failed to create pipeline layout for effect file " << effect.source_file << '!';
		return false;
	}

	api::buffer_range cb_range = {};
	std::vector<api::descriptor_set_update> descriptor_writes;
	descriptor_writes.reserve(effect.module.num_sampler_bindings + effect.module.num_texture_bindings + effect.module.num_storage_bindings + 1);
	std::vector<api::sampler_with_resource_view> sampler_descriptors;
	sampler_descriptors.resize(effect.module.num_sampler_bindings + effect.module.num_texture_bindings);

	// Create global constant buffer (except in D3D9, which does not have constant buffers)
	if (_renderer_id != 0x9000 && !effect.uniform_data_storage.empty())
	{
		if (!_device->create_resource(
				api::resource_desc(effect.uniform_data_storage.size(), api::memory_heap::cpu_to_gpu, api::resource_usage::constant_buffer),
				nullptr, api::resource_usage::cpu_access, &effect.cb))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create constant buffer for effect file " << effect.source_file << '!';
			return false;
		}

		_device->set_resource_name(effect.cb, "ReShade constant buffer");

		if (!_device->allocate_descriptor_set(effect.layout, 0, &effect.cb_set))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create constant buffer descriptor set for effect file " << effect.source_file << '!';
			return false;
		}

		cb_range.buffer = effect.cb;

		api::descriptor_set_update &write = descriptor_writes.emplace_back();
		write.set = effect.cb_set;
		write.binding = 0;
		write.type = api::descriptor_type::constant_buffer;
		write.count = 1;
		write.descriptors = &cb_range;
	}

	// Initialize bindings
	size_t total_pass_count = 0;
	for (const reshadefx::technique_info &info : effect.module.techniques)
		total_pass_count += info.passes.size();

	std::vector<api::descriptor_set> texture_sets(total_pass_count);
	std::vector<api::descriptor_set> storage_sets(total_pass_count);

	if (effect.module.num_sampler_bindings != 0)
	{
		if (!_device->allocate_descriptor_sets(static_cast<uint32_t>(sampler_with_resource_view ? total_pass_count : 1), effect.layout, 1, sampler_with_resource_view ? texture_sets.data() : &effect.sampler_set))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create sampler descriptor set for effect file " << effect.source_file << '!';
			return false;
		}

		if (!sampler_with_resource_view)
		{
			uint16_t sampler_list = 0;
			for (const reshadefx::sampler_info &info : effect.module.samplers)
			{
				// Only initialize sampler if it has not been created before
				if (0 != (sampler_list & (1 << info.binding)))
					continue;

				assert(info.binding < 16);
				sampler_list |= (1 << info.binding); // Maximum sampler slot count is 16, so a 16-bit integer is enough to hold all bindings

				api::sampler_desc desc;
				desc.filter = static_cast<api::filter_mode>(info.filter);
				desc.address_u = static_cast<api::texture_address_mode>(info.address_u);
				desc.address_v = static_cast<api::texture_address_mode>(info.address_v);
				desc.address_w = static_cast<api::texture_address_mode>(info.address_w);
				desc.mip_lod_bias = info.lod_bias;
				desc.max_anisotropy = 1;
				desc.compare_op = api::compare_op::always;
				desc.border_color[0] = 0.0f;
				desc.border_color[1] = 0.0f;
				desc.border_color[2] = 0.0f;
				desc.border_color[3] = 0.0f;
				desc.min_lod = info.min_lod;
				desc.max_lod = info.max_lod;

				api::sampler &sampler_handle = sampler_descriptors[info.binding].sampler;
				if (!create_effect_sampler_state(desc, sampler_handle))
				{
					effect.compiled = false;
					_last_reload_successfull = false;

					LOG(ERROR) << "Failed to create sampler object '" << info.unique_name << "' in " << effect.source_file << '!';
					return false;
				}

				api::descriptor_set_update &write = descriptor_writes.emplace_back();
				write.set = effect.sampler_set;
				write.binding = info.binding;
				write.type = api::descriptor_type::sampler;
				write.count = 1;
				write.descriptors = &sampler_handle;
			}
		}
	}

	if (effect.module.num_texture_bindings != 0)
	{
		assert(!sampler_with_resource_view);

		if (!_device->allocate_descriptor_sets(static_cast<uint32_t>(total_pass_count), effect.layout, 2, texture_sets.data()))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create texture descriptor set for effect file " << effect.source_file << '!';
			return false;
		}
	}

	if (effect.module.num_storage_bindings != 0)
	{
		if (!_device->allocate_descriptor_sets(static_cast<uint32_t>(total_pass_count), effect.layout, sampler_with_resource_view ? 2 : 3, storage_sets.data()))
		{
			effect.compiled = false;
			_last_reload_successfull = false;

			LOG(ERROR) << "Failed to create storage descriptor set for effect file " << effect.source_file << '!';
			return false;
		}
	}

	// Initialize techniques and passes
	size_t total_pass_index = 0;
	size_t technique_index_in_effect = 0;

	for (technique &tech : _techniques)
	{
		if (!tech.passes_data.empty() || tech.effect_index != effect_index)
			continue;

		tech.passes_data.resize(tech.passes.size());

		// Offset index so that a query exists for each command frame and two subsequent ones are used for before/after stamps
		tech.query_base_index = static_cast<uint32_t>(technique_index_in_effect++ * 2 * 4);

		for (size_t pass_index = 0; pass_index < tech.passes.size(); ++pass_index, ++total_pass_index)
		{
			reshadefx::pass_info &pass_info = tech.passes[pass_index];
			technique::pass_data &pass_data = tech.passes_data[pass_index];

			std::vector<api::pipeline_subobject> subobjects;

			if (!pass_info.cs_entry_point.empty())
			{
				api::shader_desc cs_desc = {};
				const std::string &cs = effect.assembly.at(pass_info.cs_entry_point);
				cs_desc.code = cs.data();
				cs_desc.code_size = cs.size();
				if (_renderer_id & 0x20000)
				{
					cs_desc.entry_point = pass_info.cs_entry_point.c_str();
					cs_desc.spec_constants = static_cast<uint32_t>(effect.module.spec_constants.size());
					cs_desc.spec_constant_ids = spec_constants.data();
					cs_desc.spec_constant_values = spec_data.data();
				}

				subobjects.push_back({ api::pipeline_subobject_type::compute_shader, 1, &cs_desc });

				if (!_device->create_pipeline(effect.layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &pass_data.pipeline))
				{
					effect.errors += "error: internal compiler error";
					effect.compiled = false;
					_last_reload_successfull = false;

					LOG(ERROR) << "Failed to create compute pipeline for pass " << pass_index << " in technique '" << tech.name << "' in " << effect.source_file << '!';
					return false;
				}
			}
			else
			{
				api::shader_desc vs_desc = {};
				const std::string &vs = effect.assembly.at(pass_info.vs_entry_point);
				vs_desc.code = vs.data();
				vs_desc.code_size = vs.size();
				if (_renderer_id & 0x20000)
				{
					vs_desc.entry_point = pass_info.vs_entry_point.c_str();
					vs_desc.spec_constants = static_cast<uint32_t>(effect.module.spec_constants.size());
					vs_desc.spec_constant_ids = spec_constants.data();
					vs_desc.spec_constant_values = spec_data.data();
				}

				subobjects.push_back({ api::pipeline_subobject_type::vertex_shader, 1, &vs_desc });

				api::shader_desc ps_desc = {};
				const std::string &ps = effect.assembly.at(pass_info.ps_entry_point);
				ps_desc.code = ps.data();
				ps_desc.code_size = ps.size();
				if (_renderer_id & 0x20000)
				{
					ps_desc.entry_point = pass_info.ps_entry_point.c_str();
					ps_desc.spec_constants = static_cast<uint32_t>(effect.module.spec_constants.size());
					ps_desc.spec_constant_ids = spec_constants.data();
					ps_desc.spec_constant_values = spec_data.data();
				}

				subobjects.push_back({ api::pipeline_subobject_type::pixel_shader, 1, &ps_desc });

				assert(pass_info.srgb_write_enable < 2);

				api::format render_target_formats[8] = {};

				if (pass_info.render_target_names[0].empty())
				{
					pass_info.viewport_width = _effect_width;
					pass_info.viewport_height = _effect_height;

					render_target_formats[0] = api::format_to_default_typed(_effect_color_format, pass_info.srgb_write_enable);

					subobjects.push_back({ api::pipeline_subobject_type::render_target_formats, 1, &render_target_formats[0] });
				}
				else
				{
					int render_target_count = 0;
					for (; render_target_count < 8 && !pass_info.render_target_names[render_target_count].empty(); ++render_target_count)
					{
						const auto render_target_texture = std::find_if(_textures.cbegin(), _textures.cend(),
							[&unique_name = pass_info.render_target_names[render_target_count]](const texture &item) {
								return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty());
							});
						assert(render_target_texture != _textures.cend());
						assert(render_target_texture->semantic.empty() && render_target_texture->rtv[pass_info.srgb_write_enable] != 0);

						if (std::find(pass_data.modified_resources.cbegin(), pass_data.modified_resources.cend(), render_target_texture->resource) == pass_data.modified_resources.cend())
						{
							pass_data.modified_resources.push_back(render_target_texture->resource);

							if (pass_info.generate_mipmaps && render_target_texture->levels > 1)
								pass_data.generate_mipmap_views.push_back(render_target_texture->srv[pass_info.srgb_write_enable]);
						}

						const api::resource_desc res_desc = _device->get_resource_desc(render_target_texture->resource);

						render_target_formats[render_target_count] = api::format_to_default_typed(res_desc.texture.format, pass_info.srgb_write_enable);

						pass_data.render_target_views[render_target_count] = render_target_texture->rtv[pass_info.srgb_write_enable];
					}

					subobjects.push_back({ api::pipeline_subobject_type::render_target_formats, static_cast<uint32_t>(render_target_count), render_target_formats });
				}

				// Only need to attach stencil if stencil is actually used in this pass
				if (pass_info.stencil_enable &&
					pass_info.viewport_width == _effect_width &&
					pass_info.viewport_height == _effect_height)
				{
					subobjects.push_back({ api::pipeline_subobject_type::depth_stencil_format, 1, &_effect_stencil_format });
				}

				subobjects.push_back({ api::pipeline_subobject_type::max_vertex_count, 1, &pass_info.num_vertices });
				api::primitive_topology topology = static_cast<api::primitive_topology>(pass_info.topology);
				subobjects.push_back({ api::pipeline_subobject_type::primitive_topology, 1, &topology });

				const auto convert_blend_op = [](reshadefx::pass_blend_op value) {
					switch (value)
					{
					default:
					case reshadefx::pass_blend_op::add: return api::blend_op::add;
					case reshadefx::pass_blend_op::subtract: return api::blend_op::subtract;
					case reshadefx::pass_blend_op::rev_subtract: return api::blend_op::reverse_subtract;
					case reshadefx::pass_blend_op::min: return api::blend_op::min;
					case reshadefx::pass_blend_op::max: return api::blend_op::max;
					}
				};
				const auto convert_blend_func = [](reshadefx::pass_blend_func value) {
					switch (value) {
					case reshadefx::pass_blend_func::zero: return api::blend_factor::zero;
					default:
					case reshadefx::pass_blend_func::one: return api::blend_factor::one;
					case reshadefx::pass_blend_func::src_color: return api::blend_factor::source_color;
					case reshadefx::pass_blend_func::src_alpha: return api::blend_factor::source_alpha;
					case reshadefx::pass_blend_func::inv_src_color: return api::blend_factor::one_minus_source_color;
					case reshadefx::pass_blend_func::inv_src_alpha: return api::blend_factor::one_minus_source_alpha;
					case reshadefx::pass_blend_func::dst_color: return api::blend_factor::dest_color;
					case reshadefx::pass_blend_func::dst_alpha: return api::blend_factor::dest_alpha;
					case reshadefx::pass_blend_func::inv_dst_color: return api::blend_factor::one_minus_dest_color;
					case reshadefx::pass_blend_func::inv_dst_alpha: return api::blend_factor::one_minus_dest_alpha;
					}
				};

				// Technically should check for 'api::device_caps::independent_blend' support, but render target write masks are supported in D3D9, when rest is not, so just always set ...
				api::blend_desc blend_state = {};
				for (int i = 0; i < 8; ++i)
				{
					blend_state.blend_enable[i] = pass_info.blend_enable[i];
					blend_state.source_color_blend_factor[i] = convert_blend_func(pass_info.src_blend[i]);
					blend_state.dest_color_blend_factor[i] = convert_blend_func(pass_info.dest_blend[i]);
					blend_state.color_blend_op[i] = convert_blend_op(pass_info.blend_op[i]);
					blend_state.source_alpha_blend_factor[i] = convert_blend_func(pass_info.src_blend_alpha[i]);
					blend_state.dest_alpha_blend_factor[i] = convert_blend_func(pass_info.dest_blend_alpha[i]);
					blend_state.alpha_blend_op[i] = convert_blend_op(pass_info.blend_op_alpha[i]);
					blend_state.render_target_write_mask[i] = pass_info.color_write_mask[i];
				}

				subobjects.push_back({ api::pipeline_subobject_type::blend_state, 1, &blend_state });

				api::rasterizer_desc rasterizer_state = {};
				rasterizer_state.cull_mode = api::cull_mode::none;

				subobjects.push_back({ api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_state });

				const auto convert_stencil_op = [](reshadefx::pass_stencil_op value) {
					switch (value) {
					case reshadefx::pass_stencil_op::zero: return api::stencil_op::zero;
					default:
					case reshadefx::pass_stencil_op::keep: return api::stencil_op::keep;
					case reshadefx::pass_stencil_op::invert: return api::stencil_op::invert;
					case reshadefx::pass_stencil_op::replace: return api::stencil_op::replace;
					case reshadefx::pass_stencil_op::incr: return api::stencil_op::increment;
					case reshadefx::pass_stencil_op::incr_sat: return api::stencil_op::increment_saturate;
					case reshadefx::pass_stencil_op::decr: return api::stencil_op::decrement;
					case reshadefx::pass_stencil_op::decr_sat: return api::stencil_op::decrement_saturate;
					}
				};
				const auto convert_stencil_func = [](reshadefx::pass_stencil_func value) {
					switch (value)
					{
					case reshadefx::pass_stencil_func::never: return api::compare_op::never;
					case reshadefx::pass_stencil_func::equal: return api::compare_op::equal;
					case reshadefx::pass_stencil_func::not_equal: return api::compare_op::not_equal;
					case reshadefx::pass_stencil_func::less: return api::compare_op::less;
					case reshadefx::pass_stencil_func::less_equal: return api::compare_op::less_equal;
					case reshadefx::pass_stencil_func::greater: return api::compare_op::greater;
					case reshadefx::pass_stencil_func::greater_equal: return api::compare_op::greater_equal;
					default:
					case reshadefx::pass_stencil_func::always: return api::compare_op::always;
					}
				};

				api::depth_stencil_desc depth_stencil_state = {};
				depth_stencil_state.depth_enable = false;
				depth_stencil_state.depth_write_mask = false;
				depth_stencil_state.depth_func = api::compare_op::always;
				depth_stencil_state.stencil_enable = pass_info.stencil_enable;
				depth_stencil_state.stencil_read_mask = pass_info.stencil_read_mask;
				depth_stencil_state.stencil_write_mask = pass_info.stencil_write_mask;
				depth_stencil_state.back_stencil_fail_op = convert_stencil_op(pass_info.stencil_op_fail);
				depth_stencil_state.back_stencil_depth_fail_op = convert_stencil_op(pass_info.stencil_op_depth_fail);
				depth_stencil_state.back_stencil_pass_op = convert_stencil_op(pass_info.stencil_op_pass);
				depth_stencil_state.back_stencil_func = convert_stencil_func(pass_info.stencil_comparison_func);
				depth_stencil_state.front_stencil_fail_op = depth_stencil_state.back_stencil_fail_op;
				depth_stencil_state.front_stencil_depth_fail_op = depth_stencil_state.back_stencil_depth_fail_op;
				depth_stencil_state.front_stencil_pass_op = depth_stencil_state.back_stencil_pass_op;
				depth_stencil_state.front_stencil_func = depth_stencil_state.back_stencil_func;

				subobjects.push_back({ api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_state });

				if (!_device->create_pipeline(effect.layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &pass_data.pipeline))
				{
					effect.errors += "error: internal compiler error";
					effect.compiled = false;
					_last_reload_successfull = false;

					LOG(ERROR) << "Failed to create graphics pipeline for pass " << pass_index << " in technique '" << tech.name << "' in " << effect.source_file << '!';
					return false;
				}
			}

			if (effect.module.num_sampler_bindings != 0 ||
				effect.module.num_texture_bindings != 0)
			{
				pass_data.texture_set = texture_sets[total_pass_index];

				for (const reshadefx::sampler_info &info : pass_info.samplers)
				{
					const auto sampler_texture = std::find_if(_textures.cbegin(), _textures.cend(),
						[&unique_name = info.texture_name](const texture &item) {
							return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty());
						});
					assert(sampler_texture != _textures.cend());

					api::resource_view &srv = sampler_descriptors[sampler_with_resource_view ? info.binding : effect.module.num_sampler_bindings + info.texture_binding].view;

					api::descriptor_set_update &write = descriptor_writes.emplace_back();
					write.set = pass_data.texture_set;
					write.count = 1;

					if (sampler_with_resource_view)
					{
						write.binding = info.binding;
						write.type = api::descriptor_type::sampler_with_resource_view;
						write.descriptors = &sampler_descriptors[info.binding];

						api::sampler_desc desc;
						desc.filter = static_cast<api::filter_mode>(info.filter);
						desc.address_u = static_cast<api::texture_address_mode>(info.address_u);
						desc.address_v = static_cast<api::texture_address_mode>(info.address_v);
						desc.address_w = static_cast<api::texture_address_mode>(info.address_w);
						desc.mip_lod_bias = info.lod_bias;
						desc.max_anisotropy = 1;
						desc.compare_op = api::compare_op::always;
						desc.border_color[0] = 0.0f;
						desc.border_color[1] = 0.0f;
						desc.border_color[2] = 0.0f;
						desc.border_color[3] = 0.0f;
						desc.min_lod = info.min_lod;
						desc.max_lod = info.max_lod;

						if (!create_effect_sampler_state(desc, sampler_descriptors[info.binding].sampler))
						{
							effect.compiled = false;
							_last_reload_successfull = false;

							LOG(ERROR) << "Failed to create sampler object '" << info.unique_name << "' in " << effect.source_file << '!';
							return false;
						}
					}
					else
					{
						write.binding = info.texture_binding;
						write.type = api::descriptor_type::shader_resource_view;
						write.descriptors = &srv;
					}

					if (!sampler_texture->semantic.empty())
					{
						if (const auto it = _texture_semantic_bindings.find(sampler_texture->semantic); it != _texture_semantic_bindings.end())
							srv = info.srgb ? it->second.second : it->second.first;
						else
							srv = _empty_srv;

						// Keep track of the texture descriptor to simplify updating it
						effect.texture_semantic_to_binding.push_back({
							sampler_texture->semantic,
							write.set,
							write.binding,
							sampler_with_resource_view ? sampler_descriptors[info.binding].sampler : api::sampler { 0 },
							!!info.srgb
						});
					}
					else
					{
						assert(info.srgb < 2);

						srv = sampler_texture->srv[info.srgb];
					}

					assert(srv != 0);
				}
			}

			if (effect.module.num_storage_bindings != 0)
			{
				pass_data.storage_set = storage_sets[total_pass_index];

				for (const reshadefx::storage_info &info : pass_info.storages)
				{
					const auto storage_texture = std::find_if(_textures.cbegin(), _textures.cend(),
						[&unique_name = info.texture_name](const texture &item) {
							return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty());
						});
					assert(storage_texture != _textures.cend());
					assert(storage_texture->semantic.empty() && storage_texture->uav[info.level] != 0);

					if (std::find(pass_data.modified_resources.cbegin(), pass_data.modified_resources.cend(), storage_texture->resource) == pass_data.modified_resources.cend())
					{
						pass_data.modified_resources.push_back(storage_texture->resource);

						if (pass_info.generate_mipmaps && storage_texture->levels > 1)
							pass_data.generate_mipmap_views.push_back(storage_texture->srv[0]);
					}

					api::descriptor_set_update &write = descriptor_writes.emplace_back();
					write.set = pass_data.storage_set;
					write.binding = info.binding;
					write.type = api::descriptor_type::unordered_access_view;
					write.count = 1;
					write.descriptors = &storage_texture->uav[info.level];
				}
			}
		}
	}

	if (!descriptor_writes.empty())
		_device->update_descriptor_sets(static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data());

	// Clear effect assembly now that it was consumed
	effect.assembly.clear();

	return true;
}
bool reshade::runtime::create_effect_sampler_state(const api::sampler_desc &desc, api::sampler &sampler)
{
	// Generate hash for sampler description
	size_t desc_hash = 2166136261;
	for (int i = 0; i < sizeof(desc); ++i)
		desc_hash = (desc_hash * 16777619) ^ reinterpret_cast<const uint8_t *>(&desc)[i];

	if (const auto it = _effect_sampler_states.find(desc_hash);
		it != _effect_sampler_states.end())
	{
		sampler = it->second;
		return true;
	}

	if (_device->create_sampler(desc, &sampler))
	{
		_effect_sampler_states.emplace(desc_hash, sampler);
		return true;
	}
	else
	{
		return false;
	}
}
void reshade::runtime::destroy_effect(size_t effect_index)
{
	assert(effect_index < _effects.size());

	// Make sure no effect resources are currently in use
	_graphics_queue->wait_idle();

	for (technique &tech : _techniques)
	{
		if (tech.effect_index != effect_index)
			continue;

		for (const technique::pass_data &pass : tech.passes_data)
		{
			_device->destroy_pipeline(pass.pipeline);

			_device->free_descriptor_set(pass.texture_set);
			_device->free_descriptor_set(pass.storage_set);
		}

		tech.passes_data.clear();
	}

	{	effect &effect = _effects[effect_index];

		_device->destroy_resource(effect.cb);
		effect.cb = {};

		_device->free_descriptor_set(effect.cb_set);
		effect.cb_set = {};
		_device->free_descriptor_set(effect.sampler_set);
		effect.sampler_set = {};

		_device->destroy_pipeline_layout(effect.layout);
		effect.layout = {};

		_device->destroy_query_pool(effect.query_pool);
		effect.query_pool = {};

		effect.texture_semantic_to_binding.clear();
	}

#if RESHADE_GUI
	_preview_texture.handle = 0;
	_effect_filter[0] = '\0'; // And reset filter too, since the list of techniques might have changed
#endif

	// Lock here to be safe in case another effect is still loading
	const std::unique_lock<std::shared_mutex> lock(_reload_mutex);

	// No techniques from this effect are rendering anymore
	_effects[effect_index].rendering = 0;

	// Destroy textures belonging to this effect
	_textures.erase(std::remove_if(_textures.begin(), _textures.end(),
		[this, effect_index](texture &tex) {
			tex.shared.erase(std::remove(tex.shared.begin(), tex.shared.end(), effect_index), tex.shared.end());
			if (tex.shared.empty()) {
				destroy_texture(tex);
				return true;
			}
			return false;
		}), _textures.end());
	// Clean up techniques belonging to this effect
	for (auto it = _techniques.begin(); it != _techniques.end();)
	{
		if (it->effect_index == effect_index)
		{
			const size_t technique_index = std::distance(_techniques.begin(), it);
			it = _techniques.erase(it);

			_technique_sorting.erase(std::remove(_technique_sorting.begin(), _technique_sorting.end(), technique_index), _technique_sorting.end());
			std::for_each(_technique_sorting.begin(), _technique_sorting.end(),
				[technique_index](size_t &current_technique_index) {
					if (current_technique_index > technique_index)
						current_technique_index--;
				});
		}
		else
		{
			++it;
		}
	}

	// Do not clear effect here, since it is common to be re-used immediately
}

void reshade::runtime::load_textures()
{
	for (texture &tex : _textures)
	{
		if (tex.resource == 0 || !tex.semantic.empty())
			continue; // Ignore textures that are not created yet and those that are handled in the runtime implementation

		std::filesystem::path source_path = std::filesystem::u8path(tex.annotation_as_string("source"));
		// Ignore textures that have no image file attached to them (e.g. plain render targets)
		if (source_path.empty())
			continue;

		// Search for image file using the provided search paths unless the path provided is already absolute
		if (!find_file(_texture_search_paths, source_path))
		{
			if (_effects[tex.effect_index].errors.find(source_path.u8string()) == std::string::npos)
				_effects[tex.effect_index].errors += "warning: " + tex.unique_name + ": source \"" + source_path.u8string() + "\" was not found.\n";

			LOG(ERROR) << "Source " << source_path << " for texture '" << tex.unique_name << "' was not found in any of the texture search paths!";
			continue;
		}

		std::error_code ec;
		const uintmax_t file_size = std::filesystem::file_size(source_path, ec);
		if (ec)
		{
			if (_effects[tex.effect_index].errors.find(source_path.u8string()) == std::string::npos)
				_effects[tex.effect_index].errors += "warning: " + tex.unique_name + ": source \"" + source_path.u8string() + "\" could not be loaded.\n";

			LOG(ERROR) << "Failed to load " << source_path << " for texture '" << tex.unique_name << "' with error code " << ec.value() << '!';
			continue;
		}

		stbi_uc *pixels = nullptr;
		int width = 0, height = 0, channels = 0;

		if (auto file = std::ifstream(source_path, std::ios::binary))
		{
			// Read texture data into memory in one go since that is faster than reading chunk by chunk
			std::vector<stbi_uc> file_data(static_cast<size_t>(file_size));
			file.read(reinterpret_cast<char *>(file_data.data()), file_data.size());
			file.close();

			if (stbi_dds_test_memory(file_data.data(), static_cast<int>(file_data.size())))
				pixels = stbi_dds_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &width, &height, &channels, STBI_rgb_alpha);
			else
				pixels = stbi_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &width, &height, &channels, STBI_rgb_alpha);
		}

		if (pixels == nullptr)
		{
			if (_effects[tex.effect_index].errors.find(source_path.u8string()) == std::string::npos)
				_effects[tex.effect_index].errors += "warning: " + tex.unique_name + ": source \"" + source_path.u8string() + "\" could not be loaded.\n";

			LOG(ERROR) << "Failed to load " << source_path << " for texture '" << tex.unique_name << "'! Make sure it is of a compatible file format.";
			continue;
		}

		update_texture(tex, width, height, pixels);

		stbi_image_free(pixels);

		tex.loaded = true;
	}

	_textures_loaded = true;
}
bool reshade::runtime::create_texture(texture &tex)
{
	// Do not create resource if it is a special reference, those are set in 'render_technique' and 'update_texture_bindings'
	if (!tex.semantic.empty())
		return true;

	api::format format = api::format::unknown;
	api::format view_format = api::format::unknown;
	api::format view_format_srgb = api::format::unknown;

	switch (tex.format)
	{
	case reshadefx::texture_format::r8:
		format = api::format::r8_unorm;
		break;
	case reshadefx::texture_format::r16:
		format = api::format::r16_unorm;
		break;
	case reshadefx::texture_format::r16f:
		format = api::format::r16_float;
		break;
	case reshadefx::texture_format::r32f:
		format = api::format::r32_float;
		break;
	case reshadefx::texture_format::rg8:
		format = api::format::r8g8_unorm;
		break;
	case reshadefx::texture_format::rg16:
		format = api::format::r16g16_unorm;
		break;
	case reshadefx::texture_format::rg16f:
		format = api::format::r16g16_float;
		break;
	case reshadefx::texture_format::rg32f:
		format = api::format::r32g32_float;
		break;
	case reshadefx::texture_format::rgba8:
		format = api::format::r8g8b8a8_typeless;
		view_format = api::format::r8g8b8a8_unorm;
		view_format_srgb = api::format::r8g8b8a8_unorm_srgb;
		break;
	case reshadefx::texture_format::rgba16:
		format = api::format::r16g16b16a16_unorm;
		break;
	case reshadefx::texture_format::rgba16f:
		format = api::format::r16g16b16a16_float;
		break;
	case reshadefx::texture_format::rgba32f:
		format = api::format::r32g32b32a32_float;
		break;
	case reshadefx::texture_format::rgb10a2:
		format = api::format::r10g10b10a2_unorm;
		break;
	}

	if (view_format == api::format::unknown)
		view_format_srgb = view_format = format;

	api::resource_usage usage = api::resource_usage::shader_resource;
	usage |= api::resource_usage::copy_source; // For texture data download
	if (tex.semantic.empty())
		usage |= api::resource_usage::copy_dest; // For texture data upload
	if (tex.render_target)
		usage |= api::resource_usage::render_target;
	if (tex.storage_access && _renderer_id >= 0xb000)
		usage |= api::resource_usage::unordered_access;

	api::resource_flags flags = api::resource_flags::none;
	if (tex.levels > 1)
		flags |= api::resource_flags::generate_mipmaps;

	// Clear texture to zero since by default its contents are undefined
	std::vector<uint8_t> zero_data(static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height) * 16);
	std::vector<api::subresource_data> initial_data(tex.levels);
	for (uint32_t level = 0, width = tex.width; level < tex.levels; ++level, width /= 2)
	{
		initial_data[level].data = zero_data.data();
		initial_data[level].row_pitch = width * 16;
	}

	if (!_device->create_resource(api::resource_desc(tex.width, tex.height, 1, tex.levels, format, 1, api::memory_heap::gpu_only, usage, flags), initial_data.data(), api::resource_usage::shader_resource, &tex.resource))
	{
		LOG(ERROR) << "Failed to create texture '" << tex.unique_name << "' (width = " << tex.width << ", height = " << tex.height << ", levels = " << tex.levels << ", format = " << static_cast<uint32_t>(format) << ", usage = " << std::hex << static_cast<uint32_t>(usage) << std::dec << ")! Make sure the texture dimensions are reasonable.";
		return false;
	}

	_device->set_resource_name(tex.resource, tex.unique_name.c_str());

	// Always create shader resource views
	{
		if (!_device->create_resource_view(tex.resource, api::resource_usage::shader_resource, api::resource_view_desc(view_format, 0, tex.levels, 0, 1), &tex.srv[0]))
		{
			LOG(ERROR) << "Failed to create shader resource view for texture '" << tex.unique_name << "' (format = " << static_cast<uint32_t>(view_format) << ", levels = " << tex.levels << ")!";
			return false;
		}
		if (view_format_srgb == view_format || tex.storage_access) // sRGB formats do not support storage usage
		{
			tex.srv[1] = tex.srv[0];
		}
		else if (!_device->create_resource_view(tex.resource, api::resource_usage::shader_resource, api::resource_view_desc(view_format_srgb, 0, tex.levels, 0, 1), &tex.srv[1]))
		{
			LOG(ERROR) << "Failed to create shader resource view for texture '" << tex.unique_name << "' (format = " << static_cast<uint32_t>(view_format_srgb) << ", levels = " << tex.levels << ")!";
			return false;
		}
	}

	// Create render target views (with a single level)
	if (tex.render_target)
	{
		if (!_device->create_resource_view(tex.resource, api::resource_usage::render_target, api::resource_view_desc(view_format), &tex.rtv[0]))
		{
			LOG(ERROR) << "Failed to create render target view for texture '" << tex.unique_name << "' (format = " << static_cast<uint32_t>(view_format) << ")!";
			return false;
		}
		if (view_format_srgb == view_format || tex.storage_access) // sRGB formats do not support storage usage
		{
			tex.rtv[1] = tex.rtv[0];
		}
		else if (!_device->create_resource_view(tex.resource, api::resource_usage::render_target, api::resource_view_desc(view_format_srgb), &tex.rtv[1]))
		{
			LOG(ERROR) << "Failed to create render target view for texture '" << tex.unique_name << "' (format = " << static_cast<uint32_t>(view_format_srgb) << ")!";
			return false;
		}
	}

	if (tex.storage_access && _renderer_id >= 0xb000)
	{
		tex.uav.resize(tex.levels);

		for (uint16_t level = 0; level < tex.levels; ++level)
		{
			if (!_device->create_resource_view(tex.resource, api::resource_usage::unordered_access, api::resource_view_desc(view_format, level, 1, 0, 1), &tex.uav[level]))
			{
				LOG(ERROR) << "Failed to create unordered access view for texture '" << tex.unique_name << "' (format = " << static_cast<uint32_t>(view_format) << ", level = " << level << ")!";
				return false;
			}
		}
	}

	return true;
}
void reshade::runtime::destroy_texture(texture &tex)
{
	_device->destroy_resource(tex.resource);
	tex.resource = {};

	_device->destroy_resource_view(tex.srv[0]);
	if (tex.srv[1] != tex.srv[0])
		_device->destroy_resource_view(tex.srv[1]);
	tex.srv[0] = {};
	tex.srv[1] = {};

	_device->destroy_resource_view(tex.rtv[0]);
	if (tex.rtv[1] != tex.rtv[0])
		_device->destroy_resource_view(tex.rtv[1]);
	tex.rtv[0] = {};
	tex.rtv[1] = {};

	for (const api::resource_view uav : tex.uav)
		_device->destroy_resource_view(uav);
	tex.uav.clear();
}

void reshade::runtime::enable_technique(technique &tech)
{
	assert(tech.effect_index < _effects.size());

	if (!_effects[tech.effect_index].compiled)
		return; // Cannot enable techniques that failed to compile

#if RESHADE_ADDON
	if (!is_loading() && !_is_in_api_call)
	{
		_is_in_api_call = true;
		const bool skip = invoke_addon_event<addon_event::reshade_set_technique_state>(this, api::effect_technique { reinterpret_cast<uintptr_t>(&tech) }, true);
		_is_in_api_call = false;
		if (skip)
			return;
	}
#endif

	const bool status_changed = !tech.enabled;
	tech.enabled = true;
	tech.time_left = tech.annotation_as_int("timeout");

	// Queue effect file for initialization if it was not fully loaded yet
	if (tech.passes_data.empty() &&
		// Avoid adding the same effect multiple times to the queue if it contains multiple techniques that were enabled simultaneously
		std::find(_reload_create_queue.cbegin(), _reload_create_queue.cend(), tech.effect_index) == _reload_create_queue.cend())
		_reload_create_queue.push_back(tech.effect_index);

	if (status_changed) // Increase rendering reference count
		_effects[tech.effect_index].rendering++;
}
void reshade::runtime::disable_technique(technique &tech)
{
	assert(tech.effect_index < _effects.size());

#if RESHADE_ADDON
	if (!is_loading() && !_is_in_api_call)
	{
		_is_in_api_call = true;
		const bool skip = invoke_addon_event<addon_event::reshade_set_technique_state>(this, api::effect_technique { reinterpret_cast<uintptr_t>(&tech) }, false);
		_is_in_api_call = false;
		if (skip)
			return;
	}
#endif

	const bool status_changed = tech.enabled;
	tech.enabled = false;
	tech.time_left = 0;
	tech.average_cpu_duration.clear();
	tech.average_gpu_duration.clear();

	if (status_changed) // Decrease rendering reference count
		_effects[tech.effect_index].rendering--;
}

void reshade::runtime::reorder_techniques(std::vector<size_t> &&technique_indices)
{
	assert(technique_indices.size() == _techniques.size() && technique_indices.size() == _technique_sorting.size() &&
		std::all_of(technique_indices.cbegin(), technique_indices.cend(),
			[this](size_t technique_index) {
				return std::find(_technique_sorting.cbegin(), _technique_sorting.cend(), technique_index) != _technique_sorting.cend();
			}));

#if RESHADE_ADDON
	if (!is_loading() && !_is_in_api_call)
	{
		std::vector<api::effect_technique> techniques(technique_indices.size());
		std::transform(technique_indices.cbegin(), technique_indices.cend(), techniques.begin(),
			[this](size_t technique_index) {
				return api::effect_technique { reinterpret_cast<uint64_t>(&_techniques[technique_index]) };
			});

		_is_in_api_call = true;
		const bool skip = invoke_addon_event<addon_event::reshade_reorder_techniques>(this, techniques.size(), techniques.data());
		_is_in_api_call = false;
		if (skip)
			return;

		for (size_t i = 0; i < techniques.size(); i++)
		{
			const auto tech = reinterpret_cast<technique *>(techniques[i].handle);
			if (tech == nullptr)
				return;

			technique_indices[i] = tech - _techniques.data();
		}
	}
#endif

	_technique_sorting = std::move(technique_indices);
}

void reshade::runtime::load_effects()
{
	// Build a list of effect files by walking through the effect search paths
	const std::vector<std::filesystem::path> effect_files =
		find_files(_effect_search_paths, { L".fx" });

	if (effect_files.empty())
		return; // No effect files found, so nothing more to do

	ini_file &preset = ini_file::load_cache(_current_preset_path);

	// Have to be initialized at this point or else the threads spawned below will immediately exit without reducing the remaining effects count
	assert(_is_initialized);

	// Ensure HLSL compiler is loaded before trying to compile effects in Direct3D
	if (_d3d_compiler_module == nullptr && (_renderer_id & 0xF0000) == 0)
	{
		if ((_d3d_compiler_module = LoadLibraryW(L"d3dcompiler_47.dll")) == nullptr &&
			(_d3d_compiler_module = LoadLibraryW(L"d3dcompiler_43.dll")) == nullptr)
		{
			LOG(ERROR) << "Unable to load HLSL compiler (\"d3dcompiler_47.dll\")!";
			return;
		}
	}

	// Reload preprocessor definitions from current preset before compiling to avoid having to recompile again when preset is applied in 'update_effects'
	_preset_preprocessor_definitions.clear();
	preset.get({}, "PreprocessorDefinitions", _preset_preprocessor_definitions[{}]);
	for (const std::filesystem::path &effect_file : effect_files)
		preset.get(effect_file.filename().u8string(), "PreprocessorDefinitions", _preset_preprocessor_definitions[effect_file.filename().u8string()]);

	// Allocate space for effects which are placed in this array during the 'load_effect' call
	const size_t offset = _effects.size();
	_effects.resize(offset + effect_files.size());
	_reload_remaining_effects = effect_files.size();

	// Now that we have a list of files, load them in parallel
	// Split workload into batches instead of launching a thread for every file to avoid launch overhead and stutters due to too many threads being in flight
	const size_t num_splits = std::min<size_t>(effect_files.size(), std::max<size_t>(std::thread::hardware_concurrency(), 2u) - 1);

	// Keep track of the spawned threads, so the runtime cannot be destroyed while they are still running
	for (size_t n = 0; n < num_splits; ++n)
		_worker_threads.emplace_back([this, effect_files, offset, num_splits, n, &preset]() {
			// Abort loading when initialization state changes (indicating that 'on_reset' was called in the meantime)
			for (size_t i = 0; i < effect_files.size() && _is_initialized; ++i)
				if (i * num_splits / effect_files.size() == n)
					load_effect(effect_files[i], preset, offset + i);
		});
}
bool reshade::runtime::reload_effect(size_t effect_index)
{
#if RESHADE_GUI
	_show_splash = false; // Hide splash bar when reloading a single effect file
#endif

	const std::filesystem::path source_file = _effects[effect_index].source_file;
	destroy_effect(effect_index);

	// Make sure 'is_loading' is true while loading the effect
	_reload_remaining_effects = 1;

	return load_effect(source_file, ini_file::load_cache(_current_preset_path), effect_index, true);
}
void reshade::runtime::reload_effects()
{
	// Clear out any previous effects
	destroy_effects();

#if RESHADE_ADDON
	// Call event after destroying previous effects, so add-ons get a chance to release any handles they hold to variables and techniques
	invoke_addon_event<addon_event::reshade_reloaded_effects>(this);
#endif

#if RESHADE_GUI
	_preset_is_modified = false;
	_show_splash = true; // Always show splash bar when reloading everything
	_reload_count++;
#endif
	_last_reload_successfull = true;

	load_effects();
}
void reshade::runtime::destroy_effects()
{
	// Make sure no threads are still accessing effect data
	for (std::thread &thread : _worker_threads)
		if (thread.joinable())
			thread.join();
	_worker_threads.clear();

	// Reset the effect creation queue
	_reload_create_queue.clear();

	for (size_t effect_index = 0; effect_index < _effects.size(); ++effect_index)
		destroy_effect(effect_index);

	// Reset the effect list after all resources have been destroyed
	_effects.clear();

	// Clean up sampler objects
	for (const auto &[hash, sampler] : _effect_sampler_states)
		_device->destroy_sampler(sampler);
	_effect_sampler_states.clear();

	// Unload HLSL compiler which was previously loaded in 'load_effects' above
	if (_d3d_compiler_module)
	{
		FreeLibrary(static_cast<HMODULE>(_d3d_compiler_module));
		_d3d_compiler_module = nullptr;
	}

	// Textures and techniques should have been cleaned up by the calls to 'destroy_effect' above
	assert(_textures.empty());
	assert(_techniques.empty() && _technique_sorting.empty());

	_textures_loaded = false;
	_should_reload_effect = std::numeric_limits<size_t>::max();
}

bool reshade::runtime::load_effect_cache(const std::string &id, const std::string &type, std::string &data) const
{
	if (_no_effect_cache)
		return false;

	std::filesystem::path path = g_reshade_base_path / _effect_cache_path;
	path /= std::filesystem::u8path("reshade-" + id + '.' + type);

	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;

	std::error_code ec;
	const uintmax_t file_size = std::filesystem::file_size(path, ec);
	if (ec)
		return false;

	data.resize(static_cast<size_t>(file_size), '\0');
	file.read(data.data(), data.size());
	return !file.fail();
}
bool reshade::runtime::save_effect_cache(const std::string &id, const std::string &type, const std::string &data) const
{
	if (_no_effect_cache)
		return false;

	std::filesystem::path path = g_reshade_base_path / _effect_cache_path;
	path /= std::filesystem::u8path("reshade-" + id + '.' + type);

	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
		return false;

	file.write(data.data(), data.size());
	return !file.fail();
}
void reshade::runtime::clear_effect_cache()
{
	std::error_code ec;

	// Find all cached effect files and delete them
	for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(g_reshade_base_path / _effect_cache_path, std::filesystem::directory_options::skip_permission_denied, ec))
	{
		if (entry.is_directory(ec))
			continue;

		const std::filesystem::path filename = entry.path().filename();
		const std::filesystem::path extension = entry.path().extension();
		if (filename.native().compare(0, 8, L"reshade-") != 0 || (extension != L".i" && extension != L".cso" && extension != L".asm"))
			continue;

		std::filesystem::remove(entry, ec);
	}

	if (ec)
		LOG(ERROR) << "Failed to clear effect cache directory with error code " << ec.value() << '!';
}

bool reshade::runtime::update_effect_color_and_stencil_tex(uint32_t width, uint32_t height, api::format color_format, api::format stencil_format)
{
	assert(width != 0 && height != 0);
	assert(color_format != api::format::unknown && stencil_format != api::format::unknown);

	color_format = api::format_to_typeless(color_format);

	if (_effect_color_tex != 0)
	{
		if (_effect_width == width && _effect_height == height && _effect_color_format == color_format && _effect_stencil_format == stencil_format)
			return true;

		_graphics_queue->wait_idle();

		_device->destroy_resource(_effect_color_tex);
		_effect_color_tex = {};
		_device->destroy_resource_view(_effect_color_srv[0]);
		_effect_color_srv[0] = {};
		_device->destroy_resource_view(_effect_color_srv[1]);
		_effect_color_srv[1] = {};

		_device->destroy_resource(_effect_stencil_tex);
		_effect_stencil_tex = {};
		_device->destroy_resource_view(_effect_stencil_dsv);
		_effect_stencil_dsv = {};
	}

	if (!_device->create_resource(
			api::resource_desc(width, height, 1, 1, color_format, 1, api::memory_heap::gpu_only, api::resource_usage::copy_dest | api::resource_usage::shader_resource),
			nullptr, api::resource_usage::shader_resource, &_effect_color_tex))
	{
		LOG(ERROR) << "Failed to create effect color resource (width = " << width << ", height = " << height << ", format = " << static_cast<uint32_t>(color_format) << ")!";
		return false;
	}

#if RESHADE_ADDON
	// Reload effects to update 'BUFFER_WIDTH', 'BUFFER_HEIGHT' and 'BUFFER_COLOR_BIT_DEPTH' definitions (unless this is the 'update_effect_color_and_stencil_tex' call in 'on_init')
	const bool force_reload = _is_initialized && (width != _effect_width || height != _effect_height || format_color_bit_depth(color_format) != format_color_bit_depth(_effect_color_format));
#endif

	_effect_width = width;
	_effect_height = height;
	_effect_color_format = color_format;

	_device->set_resource_name(_effect_color_tex, "ReShade back buffer");

	if (!_device->create_resource_view(_effect_color_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format_to_default_typed(color_format, 0)), &_effect_color_srv[0]) ||
		!_device->create_resource_view(_effect_color_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format_to_default_typed(color_format, 1)), &_effect_color_srv[1]))
	{
		LOG(ERROR) << "Failed to create effect color resource view (format = " << static_cast<uint32_t>(color_format) << ")!";
		return false;
	}

	update_texture_bindings("COLOR", _effect_color_srv[0], _effect_color_srv[1]);

#if RESHADE_ADDON
	if (force_reload)
	{
		if (_effects.size() == _should_reload_effect)
		{
#if RESHADE_VERBOSE_LOG
			LOG(WARN) << "Effects were rendered to different render targets with mismatching format or dimensions. This requires ReShade to recreate resources every frame which is very slow.";
#endif

			// Avoid reloading effects when effect color resource changes every frame
			_should_reload_effect = std::numeric_limits<size_t>::max();
		}
		else
		{
			_should_reload_effect = _effects.size();
		}
	}
#endif

	if (!_device->create_resource(
			api::resource_desc(width, height, 1, 1, stencil_format, 1, api::memory_heap::gpu_only, api::resource_usage::depth_stencil),
			nullptr, api::resource_usage::depth_stencil_write, &_effect_stencil_tex))
	{
		LOG(ERROR) << "Failed to create effect stencil resource (width = " << width << ", height = " << height << ", format = " << static_cast<uint32_t>(stencil_format) << ")!";
		return false;
	}

	_effect_stencil_format = stencil_format;

	_device->set_resource_name(_effect_stencil_tex, "ReShade effect stencil");

	if (!_device->create_resource_view(_effect_stencil_tex, api::resource_usage::depth_stencil, api::resource_view_desc(stencil_format), &_effect_stencil_dsv))
	{
		LOG(ERROR) << "Failed to create effect stencil resource view (format = " << static_cast<uint32_t>(stencil_format) << ")!";
		return false;
	}

	return true;
}

void reshade::runtime::update_effects()
{
	// Delay first load to the first render call to avoid loading while the application is still initializing
	if (_frame_count == 0 && !_no_reload_on_init && !(_no_reload_for_non_vr && !_is_vr))
		reload_effects();

	if (_should_reload_effect != std::numeric_limits<size_t>::max() && !is_loading())
	{
		save_current_preset(); // Save preset preprocessor definitions

		if (_should_reload_effect < _effects.size())
			reload_effect(_should_reload_effect);
		else
			reload_effects();

		_should_reload_effect = std::numeric_limits<size_t>::max();
	}

	if (_reload_remaining_effects == 0)
	{
		// Clear the thread list now that they all have finished
		for (std::thread &thread : _worker_threads)
			if (thread.joinable())
				thread.join(); // Threads have exited, but still need to join them prior to destruction
		_worker_threads.clear();

		// Finished loading effects, so apply preset to figure out which ones need compiling
		load_current_preset();

#if RESHADE_ADDON
		invoke_addon_event<addon_event::reshade_set_current_preset_path>(this, _current_preset_path.u8string().c_str());
#endif

		_last_reload_time = std::chrono::high_resolution_clock::now();
		_reload_remaining_effects = std::numeric_limits<size_t>::max();

		// Reset all effect loading options
		_load_option_disable_skipping = false;

#if RESHADE_GUI
		// Update all code editors after a reload
		for (editor_instance &instance : _editors)
		{
			if (const auto it = std::find_if(_effects.cbegin(), _effects.cend(),
					[&instance](const effect &effect) {
						return effect.source_file == instance.file_path || std::find(effect.included_files.begin(), effect.included_files.end(), instance.file_path) != effect.included_files.end();
					});
				it != _effects.cend())
			{
				// Set effect index again in case it was moved during the reload
				instance.effect_index = std::distance(_effects.cbegin(), it);

				if (instance.entry_point_name.empty())
					open_code_editor(instance);
				else
					// Those editors referencing assembly will be updated in a separate step below
					instance.editor.clear_text();
			}
		}
#endif
		return;
	}

	if (_reload_remaining_effects != std::numeric_limits<size_t>::max())
		return;

	if (!_reload_create_queue.empty())
	{
		// Pop an effect from the queue
		const size_t effect_index = _reload_create_queue.back();
		_reload_create_queue.pop_back();

		if (!create_effect(effect_index))
		{
			// Destroy all textures belonging to this effect
			for (texture &tex : _textures)
				if (tex.effect_index == effect_index && tex.shared.size() <= 1)
					destroy_texture(tex);
			// Disable all techniques belonging to this effect
			for (technique &tech : _techniques)
				if (tech.effect_index == effect_index)
					disable_technique(tech);

			_last_reload_successfull = false;
		}

		// An effect has changed, need to reload textures
		_textures_loaded = false;

#if RESHADE_GUI
		const effect &effect = _effects[effect_index];

		// Update assembly in all code editors after a reload
		for (editor_instance &instance : _editors)
		{
			if (!instance.generated || instance.entry_point_name.empty() || instance.file_path != effect.source_file)
				continue;

			assert(instance.effect_index == effect_index);

			if (effect.assembly_text.find(instance.entry_point_name) != effect.assembly_text.end())
				open_code_editor(instance);
		}
#endif
	}

	if (!_textures_loaded && _reload_create_queue.empty())
	{
		// Now that all effects were created, load all textures
		load_textures();

#if RESHADE_ADDON
		invoke_addon_event<addon_event::reshade_reloaded_effects>(this);
#endif
	}
}
void reshade::runtime::render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	// Do not render effects twice in a frame
	if (_effects_rendered_this_frame)
		return;
	_effects_rendered_this_frame = true;

	// Nothing to do here if effects are still loading or disabled globally
	if (is_loading() || !_effects_enabled || _techniques.empty())
		return;

#ifdef NDEBUG
	// Lock input so it cannot be modified by other threads while we are reading it here
	// TODO: This does not catch input happening between now and 'on_present'
	const std::shared_lock<std::shared_mutex> input_lock = (_input != nullptr) ?
		_input->lock() : std::shared_lock<std::shared_mutex>();
#endif

	// Update special uniform variables
	for (effect &effect : _effects)
	{
		if (!effect.rendering)
			continue;

		for (uniform &variable : effect.uniforms)
		{
			if (!_ignore_shortcuts && _input != nullptr && _input->is_key_pressed(variable.toggle_key_data, _force_shortcut_modifiers))
			{
				assert(variable.supports_toggle_key());

				// Change to next value if the associated shortcut key was pressed
				switch (variable.type.base)
				{
					case reshadefx::type::t_bool:
					{
						bool data = false;
						get_uniform_value(variable, &data);
						set_uniform_value(variable, !data);
						break;
					}
					case reshadefx::type::t_int:
					case reshadefx::type::t_uint:
					{
						int data[4] = {};
						get_uniform_value(variable, data, 4);
						const std::string_view ui_items = variable.annotation_as_string("ui_items");
						int num_items = 0;
						for (size_t offset = 0, next; (next = ui_items.find('\0', offset)) != std::string_view::npos; offset = next + 1)
							num_items++;
						data[0] = (data[0] + 1 >= num_items) ? 0 : data[0] + 1;
						set_uniform_value(variable, data, 4);
						break;
					}
				}

				save_current_preset();
			}

			switch (variable.special)
			{
				case special_uniform::frame_time:
				{
					set_uniform_value(variable, _last_frame_duration.count() * 1e-6f);
					break;
				}
				case special_uniform::frame_count:
				{
					if (variable.type.is_boolean())
						set_uniform_value(variable, (_frame_count % 2) == 0);
					else
						set_uniform_value(variable, static_cast<unsigned int>(_frame_count % UINT_MAX));
					break;
				}
				case special_uniform::random:
				{
					const int min = variable.annotation_as_int("min", 0, 0);
					const int max = variable.annotation_as_int("max", 0, RAND_MAX);
					set_uniform_value(variable, min + (std::rand() % (std::abs(max - min) + 1)));
					break;
				}
				case special_uniform::ping_pong:
				{
					const float min = variable.annotation_as_float("min", 0, 0.0f);
					const float max = variable.annotation_as_float("max", 0, 1.0f);
					const float step_min = variable.annotation_as_float("step", 0);
					const float step_max = variable.annotation_as_float("step", 1);
					float increment = step_max == 0 ? step_min : (step_min + std::fmodf(static_cast<float>(std::rand()), step_max - step_min + 1));
					const float smoothing = variable.annotation_as_float("smoothing");

					float value[2] = { 0, 0 };
					get_uniform_value(variable, value, 2);
					if (value[1] >= 0)
					{
						increment = std::max(increment - std::max(0.0f, smoothing - (max - value[0])), 0.05f);
						increment *= _last_frame_duration.count() * 1e-9f;

						if ((value[0] += increment) >= max)
							value[0] = max, value[1] = -1;
					}
					else
					{
						increment = std::max(increment - std::max(0.0f, smoothing - (value[0] - min)), 0.05f);
						increment *= _last_frame_duration.count() * 1e-9f;

						if ((value[0] -= increment) <= min)
							value[0] = min, value[1] = +1;
					}
					set_uniform_value(variable, value, 2);
					break;
				}
				case special_uniform::date:
				{
					const std::time_t t = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
					struct tm tm; localtime_s(&tm, &t);

					const int value[4] = {
						tm.tm_year + 1900,
						tm.tm_mon + 1,
						tm.tm_mday,
						tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec
					};
					set_uniform_value(variable, value, 4);
					break;
				}
				case special_uniform::timer:
				{
					const unsigned long long timer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_last_present_time - _start_time).count();
					set_uniform_value(variable, static_cast<unsigned int>(timer_ms));
					break;
				}
				case special_uniform::key:
				{
					if (_input == nullptr)
						break;

					if (const int keycode = variable.annotation_as_int("keycode");
						keycode > 7 && keycode < 256)
					{
						if (const std::string_view mode = variable.annotation_as_string("mode");
							mode == "toggle" || variable.annotation_as_int("toggle"))
						{
							bool current_value = false;
							get_uniform_value(variable, &current_value);
							if (_input->is_key_pressed(keycode))
								set_uniform_value(variable, !current_value);
						}
						else if (mode == "press")
							set_uniform_value(variable, _input->is_key_pressed(keycode));
						else
							set_uniform_value(variable, _input->is_key_down(keycode));
					}
					break;
				}
				case special_uniform::mouse_point:
				{
					if (_input == nullptr)
						break;

					set_uniform_value(variable, _input->mouse_position_x(), _input->mouse_position_y());
					break;
				}
				case special_uniform::mouse_delta:
				{
					if (_input == nullptr)
						break;

					set_uniform_value(variable, _input->mouse_movement_delta_x(), _input->mouse_movement_delta_y());
					break;
				}
				case special_uniform::mouse_button:
				{
					if (_input == nullptr)
						break;

					if (const int keycode = variable.annotation_as_int("keycode");
						keycode >= 0 && keycode < 5)
					{
						if (const std::string_view mode = variable.annotation_as_string("mode");
							mode == "toggle" || variable.annotation_as_int("toggle"))
						{
							bool current_value = false;
							get_uniform_value(variable, &current_value);
							if (_input->is_mouse_button_pressed(keycode))
								set_uniform_value(variable, !current_value);
						}
						else if (mode == "press")
							set_uniform_value(variable, _input->is_mouse_button_pressed(keycode));
						else
							set_uniform_value(variable, _input->is_mouse_button_down(keycode));
					}
					break;
				}
				case special_uniform::mouse_wheel:
				{
					if (_input == nullptr)
						break;

					const float min = variable.annotation_as_float("min");
					const float max = variable.annotation_as_float("max");
					float step = variable.annotation_as_float("step");
					if (step == 0.0f)
						step  = 1.0f;

					float value[2] = { 0, 0 };
					get_uniform_value(variable, value, 2);
					value[1] = _input->mouse_wheel_delta();
					value[0] = value[0] + value[1] * step;
					if (min != max)
					{
						value[0] = std::max(value[0], min);
						value[0] = std::min(value[0], max);
					}
					set_uniform_value(variable, value, 2);
					break;
				}
#if RESHADE_GUI
				case special_uniform::overlay_open:
				{
					set_uniform_value(variable, _show_overlay);
					break;
				}
				case special_uniform::overlay_active:
				case special_uniform::overlay_hovered:
				{
					// These are set in 'draw_variable_editor' when overlay is open
					if (!_show_overlay)
						set_uniform_value(variable, 0);
					break;
				}
#endif
				case special_uniform::screenshot:
				{
					set_uniform_value(variable, _should_save_screenshot);
					break;
				}
			}
		}
	}

	if (rtv == 0)
		return;
	if (rtv_srgb == 0)
		rtv_srgb = rtv;

	const api::resource back_buffer_resource = _device->get_resource_from_view(rtv);

#if RESHADE_ADDON
	if (!_is_in_present_call || (_effect_width != _width || _effect_height != _height || _effect_color_format != api::format_to_typeless(_back_buffer_format)))
	{
		const api::resource_desc back_buffer_desc = _device->get_resource_desc(back_buffer_resource);
		if (back_buffer_desc.texture.samples > 1)
			return; // Multisampled render targets are not supported

		// Ensure dimensions and format of the effect color resource matches that of the input back buffer resource (so that the copy to the effect color resource succeeds)
		// Changing dimensions or format can cause effects to be reloaded, in which case need to wait for that to finish before rendering
		if (!update_effect_color_and_stencil_tex(back_buffer_desc.texture.width, back_buffer_desc.texture.height, back_buffer_desc.texture.format, _effect_stencil_format))
			return;
	}

	invoke_addon_event<addon_event::reshade_begin_effects>(this, cmd_list, rtv, rtv_srgb);
#endif

#ifndef NDEBUG
	cmd_list->begin_debug_event("ReShade effects");
#endif

	// Render all enabled techniques
	for (size_t technique_index : _technique_sorting)
	{
		technique &tech = _techniques[technique_index];

		if (!_ignore_shortcuts && _input != nullptr && _input->is_key_pressed(tech.toggle_key_data, _force_shortcut_modifiers))
		{
			if (!tech.enabled)
				enable_technique(tech);
			else
				disable_technique(tech);
		}

		if (tech.passes_data.empty() || !tech.enabled || (_should_save_screenshot && !tech.enabled_in_screenshot))
			continue; // Ignore techniques that are not fully loaded or currently disabled

		render_technique(tech, cmd_list, back_buffer_resource, rtv, rtv_srgb);

		if (tech.time_left > 0)
		{
			tech.time_left -= std::chrono::duration_cast<std::chrono::milliseconds>(_last_frame_duration).count();
			if (tech.time_left <= 0)
				disable_technique(tech);
		}
	}

#ifndef NDEBUG
	cmd_list->end_debug_event();
#endif

#if RESHADE_ADDON
	invoke_addon_event<addon_event::reshade_finish_effects>(this, cmd_list, rtv, rtv_srgb);
#endif
}
void reshade::runtime::render_technique(technique &tech, api::command_list *cmd_list, api::resource back_buffer_resource, api::resource_view back_buffer_rtv, api::resource_view back_buffer_rtv_srgb)
{
	const effect &effect = _effects[tech.effect_index];

#if RESHADE_GUI
	if (_gather_gpu_statistics && effect.query_pool != 0)
	{
		// Evaluate queries from oldest frame in queue
		if (uint64_t timestamps[2];
			_device->get_query_pool_results(effect.query_pool, tech.query_base_index + ((_frame_count + 1) % 4) * 2, 2, timestamps, sizeof(uint64_t)))
			tech.average_gpu_duration.append(timestamps[1] - timestamps[0]);

		cmd_list->end_query(effect.query_pool, api::query_type::timestamp, tech.query_base_index + (_frame_count % 4) * 2);
	}

	const std::chrono::high_resolution_clock::time_point time_technique_started = std::chrono::high_resolution_clock::now();
#endif

#ifndef NDEBUG
	cmd_list->begin_debug_event(tech.name.c_str());
#endif

	// Update shader constants
	if (void *mapped_uniform_data;
		effect.cb != 0 && _device->map_buffer_region(effect.cb, 0, std::numeric_limits<uint64_t>::max(), api::map_access::write_discard, &mapped_uniform_data))
	{
		std::memcpy(mapped_uniform_data, effect.uniform_data_storage.data(), effect.uniform_data_storage.size());
		_device->unmap_buffer_region(effect.cb);
	}
	else if (_renderer_id == 0x9000)
	{
		cmd_list->push_constants(api::shader_stage::all, effect.layout, 0, 0, static_cast<uint32_t>(effect.uniform_data_storage.size() / 4), effect.uniform_data_storage.data());
	}

	const bool sampler_with_resource_view = _device->check_capability(api::device_caps::sampler_with_resource_view);

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_back_buffer_copy = true; // First pass always needs the back buffer updated

	for (size_t pass_index = 0; pass_index < tech.passes.size(); ++pass_index)
	{
		if (needs_implicit_back_buffer_copy)
		{
			// Save back buffer of previous pass
			const api::resource resources[2] = { back_buffer_resource, _effect_color_tex };
			const api::resource_usage state_old[2] = { api::resource_usage::render_target, api::resource_usage::shader_resource };
			const api::resource_usage state_new[2] = { api::resource_usage::copy_source, api::resource_usage::copy_dest };

			cmd_list->barrier(2, resources, state_old, state_new);
			cmd_list->copy_texture_region(back_buffer_resource, 0, nullptr, _effect_color_tex, 0, nullptr);
			cmd_list->barrier(2, resources, state_new, state_old);
		}

		const reshadefx::pass_info &pass_info = tech.passes[pass_index];
		const technique::pass_data &pass_data = tech.passes_data[pass_index];

#ifndef NDEBUG
		cmd_list->begin_debug_event((pass_info.name.empty() ? "Pass " + std::to_string(pass_index) : pass_info.name).c_str());
#endif

		const uint32_t num_barriers = static_cast<uint32_t>(pass_data.modified_resources.size());

		if (!pass_info.cs_entry_point.empty())
		{
			// Compute shaders do not write to the back buffer, so no update necessary
			needs_implicit_back_buffer_copy = false;

			cmd_list->bind_pipeline(api::pipeline_stage::all_compute, pass_data.pipeline);

			temp_mem<api::resource_usage> state_old, state_new;
			std::fill_n(state_old.p, num_barriers, api::resource_usage::shader_resource);
			std::fill_n(state_new.p, num_barriers, api::resource_usage::unordered_access);
			cmd_list->barrier(num_barriers, pass_data.modified_resources.data(), state_old.p, state_new.p);

			// Reset bindings on every pass (since they get invalidated by the call to 'generate_mipmaps' below)
			if (effect.cb != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_compute, effect.layout, 0, effect.cb_set);
			if (effect.sampler_set != 0)
				assert(!sampler_with_resource_view),
				cmd_list->bind_descriptor_set(api::shader_stage::all_compute, effect.layout, 1, effect.sampler_set);
			if (pass_data.texture_set != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_compute, effect.layout, sampler_with_resource_view ? 1 : 2, pass_data.texture_set);
			if (pass_data.storage_set != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_compute, effect.layout, sampler_with_resource_view ? 2 : 3, pass_data.storage_set);

			cmd_list->dispatch(pass_info.viewport_width, pass_info.viewport_height, pass_info.viewport_dispatch_z);

			cmd_list->barrier(num_barriers, pass_data.modified_resources.data(), state_new.p, state_old.p);
		}
		else
		{
			cmd_list->bind_pipeline(api::pipeline_stage::all_graphics, pass_data.pipeline);

			// Transition resource state for render targets
			temp_mem<api::resource_usage> state_old, state_new;
			std::fill_n(state_old.p, num_barriers, api::resource_usage::shader_resource);
			std::fill_n(state_new.p, num_barriers, api::resource_usage::render_target);
			cmd_list->barrier(num_barriers, pass_data.modified_resources.data(), state_old.p, state_new.p);

			// Setup render targets
			uint32_t render_target_count = 0;
			api::render_pass_depth_stencil_desc depth_stencil = {};
			api::render_pass_render_target_desc render_target[8] = {};

			if (pass_info.render_target_names[0].empty())
			{
				needs_implicit_back_buffer_copy = true;

				render_target[0].view = pass_info.srgb_write_enable ? back_buffer_rtv_srgb : back_buffer_rtv;
				render_target_count = 1;
			}
			else
			{
				needs_implicit_back_buffer_copy = false;

				for (int i = 0; i < 8 && pass_data.render_target_views[i] != 0; ++i, ++render_target_count)
					render_target[i].view = pass_data.render_target_views[i];
			}

			if (pass_info.clear_render_targets)
			{
				for (int i = 0; i < 8; ++i)
					render_target[i].load_op = api::render_pass_load_op::clear;
			}

			if (pass_info.stencil_enable &&
				pass_info.viewport_width == _effect_width &&
				pass_info.viewport_height == _effect_height)
			{
				depth_stencil.view = _effect_stencil_dsv;

				// First pass to use the stencil buffer should clear it
				if (!is_effect_stencil_cleared)
					depth_stencil.stencil_load_op = api::render_pass_load_op::clear, is_effect_stencil_cleared = true;
			}

			cmd_list->begin_render_pass(render_target_count, render_target, depth_stencil.view != 0 ? &depth_stencil : nullptr);

			// Reset bindings on every pass (since they get invalidated by the call to 'generate_mipmaps' below)
			if (effect.cb != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_graphics, effect.layout, 0, effect.cb_set);
			if (effect.sampler_set != 0)
				assert(!sampler_with_resource_view),
				cmd_list->bind_descriptor_set(api::shader_stage::all_graphics, effect.layout, 1, effect.sampler_set);
			// Setup shader resources after binding render targets, to ensure any OM bindings by the application are unset at this point (e.g. a depth buffer that was bound to the OM and is now bound as shader resource)
			if (pass_data.texture_set != 0)
				cmd_list->bind_descriptor_set(api::shader_stage::all_graphics, effect.layout, sampler_with_resource_view ? 1 : 2, pass_data.texture_set);

			const api::viewport viewport = {
				0.0f, 0.0f,
				static_cast<float>(pass_info.viewport_width),
				static_cast<float>(pass_info.viewport_height),
				0.0f, 1.0f
			};
			cmd_list->bind_viewports(0, 1, &viewport);

			const api::rect scissor_rect = {
				0, 0,
				static_cast<int32_t>(pass_info.viewport_width),
				static_cast<int32_t>(pass_info.viewport_height)
			};
			cmd_list->bind_scissor_rects(0, 1, &scissor_rect);

			if (_renderer_id == 0x9000)
			{
				// Set __TEXEL_SIZE__ constant (see effect_codegen_hlsl.cpp)
				const float texel_size[4] = {
					-1.0f / pass_info.viewport_width,
					 1.0f / pass_info.viewport_height
				};
				cmd_list->push_constants(api::shader_stage::vertex, effect.layout, 0, 255 * 4, 4, texel_size);

				// Set SEMANTIC_PIXEL_SIZE constants (see 'load_effect' above)
				uint32_t semantic_index = 0;
				for (const reshadefx::texture_info &tex : effect.module.textures)
				{
					if (tex.semantic.empty() || tex.semantic == "COLOR")
						continue;

					semantic_index++;

					if (const auto it = _texture_semantic_bindings.find(tex.semantic); it != _texture_semantic_bindings.end())
					{
						const float pixel_size[4] = {
							1.0f / _effect_width,
							1.0f / _effect_height
						};

						cmd_list->push_constants(api::shader_stage::vertex | api::shader_stage::pixel, effect.layout, 0, (255 - semantic_index) * 4, 4, pixel_size);
					}
				}
			}

			// Draw primitives
			cmd_list->draw(pass_info.num_vertices, 1, 0, 0);

			cmd_list->end_render_pass();

			// Transition resource state back to shader access
			cmd_list->barrier(num_barriers, pass_data.modified_resources.data(), state_new.p, state_old.p);
		}

		// Generate mipmaps for modified resources
		for (const api::resource_view modified_texture : pass_data.generate_mipmap_views)
			cmd_list->generate_mipmaps(modified_texture);

#ifndef NDEBUG
		cmd_list->end_debug_event();
#endif
	}

#ifndef NDEBUG
	cmd_list->end_debug_event();
#endif

#if RESHADE_GUI
	const std::chrono::high_resolution_clock::time_point time_technique_finished = std::chrono::high_resolution_clock::now();

	tech.average_cpu_duration.append(std::chrono::duration_cast<std::chrono::nanoseconds>(time_technique_finished - time_technique_started).count());

	if (_gather_gpu_statistics && effect.query_pool != 0)
		cmd_list->end_query(effect.query_pool, api::query_type::timestamp, tech.query_base_index + (_frame_count % 4) * 2 + 1);
#endif

#if RESHADE_ADDON
	if (_is_in_api_call)
		return;

	_is_in_api_call = true;
	invoke_addon_event<addon_event::reshade_render_technique>(const_cast<runtime *>(this), api::effect_technique { reinterpret_cast<uintptr_t>(&tech) }, cmd_list, back_buffer_rtv, back_buffer_rtv_srgb);
	_is_in_api_call = false;
#endif
}

void reshade::runtime::save_texture(const texture &tex)
{
	std::string filename = tex.unique_name;
	filename += (_screenshot_format == 0 ? ".bmp" : _screenshot_format == 1 ? ".png" : ".jpg");

	const std::filesystem::path screenshot_path = g_reshade_base_path / _screenshot_path / std::filesystem::u8path(filename);

	_last_screenshot_save_successfull = true;

	if (std::vector<uint8_t> pixels(static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height) * 4);
		get_texture_data(tex.resource, api::resource_usage::shader_resource, pixels.data()))
	{
		_worker_threads.emplace_back([this, screenshot_path, pixels = std::move(pixels), width = tex.width, height = tex.height]() mutable {
			// Default to a save failure unless it is reported to succeed below
			bool save_success = false;

			if (auto file = std::ofstream(screenshot_path, std::ios::binary | std::ios::trunc))
			{
				const auto write_callback = [](void *context, void *data, int size) {
					static_cast<std::ofstream *>(context)->write(static_cast<const char *>(data), size);
				};

				switch (_screenshot_format)
				{
				case 0:
					save_success = stbi_write_bmp_to_func(write_callback, &file, width, height, 4, pixels.data()) != 0;
					break;
				case 1:
				{
#if 1
					std::vector<uint8_t> encoded_data;
					save_success = fpng::fpng_encode_image_to_memory(pixels.data(), width, height, 4, encoded_data);
					write_callback(&file, encoded_data.data(), static_cast<int>(encoded_data.size()));
#else
					save_success = stbi_write_png_to_func(write_callback, &file, width, height, 4, pixels.data(), 0) != 0;
#endif
					break;
				}
				case 2:
					save_success = stbi_write_jpg_to_func(write_callback, &file, width, height, 4, pixels.data(), _screenshot_jpeg_quality) != 0;
					break;
				}

				if (!file)
					save_success = false;
			}

			if (_last_screenshot_save_successfull)
			{
				_last_screenshot_time = std::chrono::high_resolution_clock::now();
				_last_screenshot_file = screenshot_path;
				_last_screenshot_save_successfull = save_success;
			}
		});
	}
}
void reshade::runtime::update_texture(texture &tex, uint32_t width, uint32_t height, const uint8_t *pixels)
{
	std::vector<uint8_t> resized(static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height) * 4);
	// Need to potentially resize image data to the texture dimensions
	if (tex.width != width || tex.height != height)
	{
		LOG(INFO) << "Resizing image data for texture '" << tex.unique_name << "' from " << width << "x" << height << " to " << tex.width << "x" << tex.height << '.';

		stbir_resize_uint8(pixels, width, height, 0, resized.data(), tex.width, tex.height, 0, 4);
	}
	else
	{
		std::memcpy(resized.data(), pixels, resized.size());
	}

	// Collapse data to the correct number of components per pixel based on the texture format
	uint32_t row_pitch = tex.width;
	switch (tex.format)
	{
	case reshadefx::texture_format::r8:
		for (size_t i = 4, k = 1; i < resized.size(); i += 4, k += 1)
			resized[k] = resized[i];
		break;
	case reshadefx::texture_format::rg8:
		for (size_t i = 4, k = 2; i < resized.size(); i += 4, k += 2)
			resized[k + 0] = resized[i + 0],
			resized[k + 1] = resized[i + 1];
		row_pitch *= 2;
		break;
	case reshadefx::texture_format::rgba8:
		row_pitch *= 4;
		break;
	default:
		LOG(ERROR) << "Texture upload is not supported for format " << static_cast<int>(tex.format) << " of texture '" << tex.unique_name << "'!";
		return;
	}

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
	cmd_list->barrier(tex.resource, api::resource_usage::shader_resource, api::resource_usage::copy_dest);
	_device->update_texture_region({ resized.data(), row_pitch, row_pitch * tex.height }, tex.resource, 0);
	cmd_list->barrier(tex.resource, api::resource_usage::copy_dest, api::resource_usage::shader_resource);

	if (tex.levels > 1)
		cmd_list->generate_mipmaps(tex.srv[0]);
}

void reshade::runtime::reset_uniform_value(uniform &variable)
{
	if (variable.special != reshade::special_uniform::none)
	{
		std::memset(_effects[variable.effect_index].uniform_data_storage.data() + variable.offset, 0, variable.size);
		return;
	}

	static const reshadefx::constant zero = {};

	// Need to use typed setters, to ensure values are properly forced to floating point in D3D9
	for (size_t i = 0, array_length = (variable.type.is_array() ? variable.type.array_length : 1); i < array_length; ++i)
	{
		const reshadefx::constant &value = variable.has_initializer_value ? variable.type.is_array() ? variable.initializer_value.array_data[i] : variable.initializer_value : zero;

		switch (variable.type.base)
		{
		case reshadefx::type::t_int:
			set_uniform_value(variable, value.as_int, variable.type.components(), i);
			break;
		case reshadefx::type::t_bool:
		case reshadefx::type::t_uint:
			set_uniform_value(variable, value.as_uint, variable.type.components(), i);
			break;
		case reshadefx::type::t_float:
			set_uniform_value(variable, value.as_float, variable.type.components(), i);
			break;
		}
	}
}

static inline bool force_floating_point_value(const reshadefx::type &type, uint32_t renderer_id)
{
	if (renderer_id == 0x9000)
		return true; // All uniform variables are floating-point in D3D9
	if (type.is_matrix() && (renderer_id & 0x10000))
		return true; // All matrices are floating-point in GLSL
	return false;
}

void reshade::runtime::get_uniform_value_data(const uniform &variable, uint8_t *data, size_t size, size_t base_index) const
{
	size = std::min(size, static_cast<size_t>(variable.size));
	assert(data != nullptr && (size % 4) == 0);

	const std::vector<uint8_t> &data_storage = _effects[variable.effect_index].uniform_data_storage;
	assert(variable.offset + size <= data_storage.size());

	const size_t array_length = (variable.type.is_array() ? variable.type.array_length : 1);
	if (assert(base_index < array_length); base_index >= array_length)
		return;

	if (variable.type.is_matrix())
	{
		for (size_t a = base_index, i = 0; a < array_length; ++a)
			// Each row of a matrix is 16-byte aligned, so needs special handling
			for (size_t row = 0; row < variable.type.rows; ++row)
				for (size_t col = 0; i < (size / 4) && col < variable.type.cols; ++col, ++i)
					std::memcpy(
						data + ((a - base_index) * variable.type.components() + (row * variable.type.cols + col)) * 4,
						data_storage.data() + variable.offset + (a * (variable.type.rows * 4) + (row * 4 + col)) * 4, 4);
	}
	else if (array_length > 1)
	{
		for (size_t a = base_index, i = 0; a < array_length; ++a)
			// Each element in the array is 16-byte aligned, so needs special handling
			for (size_t row = 0; i < (size / 4) && row < variable.type.rows; ++row, ++i)
				std::memcpy(
					data + ((a - base_index) * variable.type.components() + row) * 4,
					data_storage.data() + variable.offset + (a * 4 + row) * 4, 4);
	}
	else
	{
		std::memcpy(data, data_storage.data() + variable.offset, size);
	}
}

template <> void reshade::runtime::get_uniform_value<bool>(const uniform &variable, bool *values, size_t count, size_t array_index) const
{
	count = std::min(count, static_cast<size_t>(variable.size / 4));
	assert(values != nullptr);

	temp_mem<uint8_t, 4 * 16> data(variable.size);
	get_uniform_value_data(variable, data.p, variable.size, array_index);

	for (size_t i = 0; i < count; ++i)
		values[i] = reinterpret_cast<const uint32_t *>(data.p)[i] != 0;
}
template <> void reshade::runtime::get_uniform_value<float>(const uniform &variable, float *values, size_t count, size_t array_index) const
{
	if (variable.type.is_floating_point() || force_floating_point_value(variable.type, _renderer_id))
	{
		get_uniform_value_data(variable, reinterpret_cast<uint8_t *>(values), count * sizeof(float), array_index);
		return;
	}

	count = std::min(count, static_cast<size_t>(variable.size / 4));
	assert(values != nullptr);

	temp_mem<uint8_t, 4 * 16> data(variable.size);
	get_uniform_value_data(variable, data.p, variable.size, array_index);

	for (size_t i = 0; i < count; ++i)
		if (variable.type.is_signed())
			values[i] = static_cast<float>(reinterpret_cast<const int32_t *>(data.p)[i]);
		else
			values[i] = static_cast<float>(reinterpret_cast<const uint32_t *>(data.p)[i]);
}
template <> void reshade::runtime::get_uniform_value<int32_t>(const uniform &variable, int32_t *values, size_t count, size_t array_index) const
{
	if (variable.type.is_integral() && !force_floating_point_value(variable.type, _renderer_id))
	{
		get_uniform_value_data(variable, reinterpret_cast<uint8_t *>(values), count * sizeof(int32_t), array_index);
		return;
	}

	count = std::min(count, static_cast<size_t>(variable.size / 4));
	assert(values != nullptr);

	temp_mem<uint8_t, 4 * 16> data(variable.size);
	get_uniform_value_data(variable, data.p, variable.size, array_index);

	for (size_t i = 0; i < count; ++i)
		values[i] = static_cast<int32_t>(reinterpret_cast<const float *>(data.p)[i]);
}
template <> void reshade::runtime::get_uniform_value<uint32_t>(const uniform &variable, uint32_t *values, size_t count, size_t array_index) const
{
	get_uniform_value(variable, reinterpret_cast<int32_t *>(values), count, array_index);
}

void reshade::runtime::set_uniform_value_data(uniform &variable, const uint8_t *data, size_t size, size_t base_index)
{
#if RESHADE_ADDON
	if (!is_loading() && !_is_in_api_call)
	{
		_is_in_api_call = true;
		const bool skip = invoke_addon_event<addon_event::reshade_set_uniform_value>(this, api::effect_uniform_variable { reinterpret_cast<uintptr_t>(&variable) }, data, size);
		_is_in_api_call = false;
		if (skip)
			return;
	}
#endif

	size = std::min(size, static_cast<size_t>(variable.size));
	assert(data != nullptr && (size % 4) == 0);

	std::vector<uint8_t> &data_storage = _effects[variable.effect_index].uniform_data_storage;
	assert(variable.offset + size <= data_storage.size());

	const size_t array_length = (variable.type.is_array() ? variable.type.array_length : 1);
	if (assert(base_index < array_length); base_index >= array_length)
		return;

	if (variable.type.is_matrix())
	{
		for (size_t a = base_index, i = 0; a < array_length; ++a)
			// Each row of a matrix is 16-byte aligned, so needs special handling
			for (size_t row = 0; row < variable.type.rows; ++row)
				for (size_t col = 0; i < (size / 4) && col < variable.type.cols; ++col, ++i)
					std::memcpy(
						data_storage.data() + variable.offset + (a * variable.type.rows * 4 + (row * 4 + col)) * 4,
						data + ((a - base_index) * variable.type.components() + (row * variable.type.cols + col)) * 4, 4);
	}
	else if (array_length > 1)
	{
		for (size_t a = base_index, i = 0; a < array_length; ++a)
			// Each element in the array is 16-byte aligned, so needs special handling
			for (size_t row = 0; i < (size / 4) && row < variable.type.rows; ++row, ++i)
				std::memcpy(
					data_storage.data() + variable.offset + (a * 4 + row) * 4,
					data + ((a - base_index) * variable.type.components() + row) * 4, 4);
	}
	else
	{
		std::memcpy(data_storage.data() + variable.offset, data, size);
	}
}

template <> void reshade::runtime::set_uniform_value<bool>(uniform &variable, const bool *values, size_t count, size_t array_index)
{
	if (variable.type.is_floating_point() || force_floating_point_value(variable.type, _renderer_id))
	{
		temp_mem<float, 16> data(count);
		for (size_t i = 0; i < count; ++i)
			data[i] = values[i] ? 1.0f : 0.0f;

		set_uniform_value_data(variable, reinterpret_cast<const uint8_t *>(data.p), count * sizeof(float), array_index);
	}
	else
	{
		temp_mem<uint32_t, 16> data(count);
		for (size_t i = 0; i < count; ++i)
			data[i] = values[i] ? 1 : 0;

		set_uniform_value_data(variable, reinterpret_cast<const uint8_t *>(data.p), count * sizeof(uint32_t), array_index);
	}
}
template <> void reshade::runtime::set_uniform_value<float>(uniform &variable, const float *values, size_t count, size_t array_index)
{
	if (variable.type.is_floating_point() || force_floating_point_value(variable.type, _renderer_id))
	{
		set_uniform_value_data(variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(float), array_index);
	}
	else
	{
		temp_mem<int32_t, 16> data(count);
		for (size_t i = 0; i < count; ++i)
			data[i] = static_cast<int32_t>(values[i]);

		set_uniform_value_data(variable, reinterpret_cast<const uint8_t *>(data.p), count * sizeof(int32_t), array_index);
	}
}
template <> void reshade::runtime::set_uniform_value<int32_t>(uniform &variable, const int32_t *values, size_t count, size_t array_index)
{
	if (variable.type.is_floating_point() || force_floating_point_value(variable.type, _renderer_id))
	{
		temp_mem<float, 16> data(count);
		for (size_t i = 0; i < count; ++i)
			data[i] = static_cast<float>(values[i]);

		set_uniform_value_data(variable, reinterpret_cast<const uint8_t *>(data.p), count * sizeof(float), array_index);
	}
	else
	{
		set_uniform_value_data(variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(int32_t), array_index);
	}
}
template <> void reshade::runtime::set_uniform_value<uint32_t>(uniform &variable, const uint32_t *values, size_t count, size_t array_index)
{
	if (variable.type.is_floating_point() || force_floating_point_value(variable.type, _renderer_id))
	{
		temp_mem<float, 16> data(count);
		for (size_t i = 0; i < count; ++i)
			data[i] = static_cast<float>(values[i]);

		set_uniform_value_data(variable, reinterpret_cast<const uint8_t *>(data.p), count * sizeof(float), array_index);
	}
	else
	{
		set_uniform_value_data(variable, reinterpret_cast<const uint8_t *>(values), count * sizeof(uint32_t), array_index);
	}
}
#endif

static std::string expand_macro_string(const std::string &input, std::vector<std::pair<std::string, std::string>> macros)
{
	const auto now = std::chrono::system_clock::now();
	const auto now_seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

	char timestamp[21];
	const std::time_t t = std::chrono::system_clock::to_time_t(now_seconds);
	struct tm tm; localtime_s(&tm, &t);

	sprintf_s(timestamp, "%.4d-%.2d-%.2d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	macros.emplace_back("Date", timestamp);
	sprintf_s(timestamp, "%.4d", tm.tm_year + 1900);
	macros.emplace_back("DateYear", timestamp);
	macros.emplace_back("Year", timestamp);
	sprintf_s(timestamp, "%.2d", tm.tm_mon + 1);
	macros.emplace_back("DateMonth", timestamp);
	macros.emplace_back("Month", timestamp);
	sprintf_s(timestamp, "%.2d", tm.tm_mday);
	macros.emplace_back("DateDay", timestamp);
	macros.emplace_back("Day", timestamp);

	sprintf_s(timestamp, "%.2d-%.2d-%.2d", tm.tm_hour, tm.tm_min, tm.tm_sec);
	macros.emplace_back("Time", timestamp);
	sprintf_s(timestamp, "%.2d", tm.tm_hour);
	macros.emplace_back("TimeHour", timestamp);
	macros.emplace_back("Hour", timestamp);
	sprintf_s(timestamp, "%.2d", tm.tm_min);
	macros.emplace_back("TimeMinute", timestamp);
	macros.emplace_back("Minute", timestamp);
	sprintf_s(timestamp, "%.2d", tm.tm_sec);
	macros.emplace_back("TimeSecond", timestamp);
	macros.emplace_back("Second", timestamp);
	sprintf_s(timestamp, "%.3lld", std::chrono::duration_cast<std::chrono::milliseconds>(now - now_seconds).count());
	macros.emplace_back("TimeMillisecond", timestamp);
	macros.emplace_back("Millisecond", timestamp);
	macros.emplace_back("TimeMS", timestamp);

	std::string result;

	for (size_t offset = 0, macro_beg, macro_end; offset < input.size(); offset = macro_end + 1)
	{
		macro_beg = input.find('%', offset);
		macro_end = input.find('%', macro_beg + 1);

		if (macro_beg == std::string::npos || macro_end == std::string::npos)
		{
			result += input.substr(offset);
			break;
		}
		else
		{
			result += input.substr(offset, macro_beg - offset);

			if (macro_end == macro_beg + 1) // Handle case of %% to escape percentage symbol
			{
				result += '%';
				continue;
			}
		}

		std::string_view replacing(input);
		replacing = replacing.substr(macro_beg + 1, macro_end - (macro_beg + 1));
		size_t colon_pos = replacing.find(':');

		std::string name;
		if (colon_pos == std::string_view::npos)
			name = replacing;
		else
			name = replacing.substr(0, colon_pos);

		std::string value;
		for (const std::pair<std::string, std::string> &macro : macros)
		{
			if (_stricmp(name.c_str(), macro.first.c_str()) == 0)
			{
				value = macro.second;
				break;
			}
		}

		if (colon_pos == std::string_view::npos)
		{
			result += value;
		}
		else
		{
			std::string_view param = replacing.substr(colon_pos + 1);

			if (const size_t insert_pos = param.find('$');
				insert_pos != std::string_view::npos)
			{
				result += param.substr(0, insert_pos);
				result += value;
				result += param.substr(insert_pos + 1);
			}
			else
			{
				result += param;
			}
		}
	}

	return result;
}

void reshade::runtime::save_screenshot(const std::string_view &postfix)
{
	const unsigned int screenshot_count = _screenshot_count;

	std::string screenshot_name = expand_macro_string(_screenshot_name, {
		{ "AppName", g_target_executable_path.stem().u8string() },
#if RESHADE_FX
		{ "PresetName",  _current_preset_path.stem().u8string() },
		{ "Count", std::to_string(screenshot_count) }
#endif
	});

	screenshot_name += postfix;
	screenshot_name += (_screenshot_format == 0 ? ".bmp" : _screenshot_format == 1 ? ".png" : ".jpg");

	const std::filesystem::path screenshot_path = g_reshade_base_path / _screenshot_path / std::filesystem::u8path(screenshot_name);

	LOG(INFO) << "Saving screenshot to " << screenshot_path << '.';

	_last_screenshot_save_successfull = true;

	if (std::vector<uint8_t> pixels(static_cast<size_t>(_width) * static_cast<size_t>(_height) * 4);
		capture_screenshot(pixels.data()))
	{
#if RESHADE_FX
		const bool include_preset = _screenshot_include_preset && postfix.empty() && ini_file::flush_cache(_current_preset_path);
#else
		const bool include_preset = false;
#endif
		// Play screenshot sound
		if (!_screenshot_sound_path.empty())
			utils::play_sound_async(g_reshade_base_path / _screenshot_sound_path);

		_worker_threads.emplace_back([this, screenshot_count, screenshot_path, pixels = std::move(pixels), include_preset]() mutable {
			// Remove alpha channel
			int comp = 4;
			if (_screenshot_clear_alpha)
			{
				comp = 3;
				for (size_t i = 0; i < static_cast<size_t>(_width) * static_cast<size_t>(_height); ++i)
					*reinterpret_cast<uint32_t *>(pixels.data() + 3 * i) = *reinterpret_cast<const uint32_t *>(pixels.data() + 4 * i);
			}

			// Create screenshot directory if it does not exist
			std::error_code ec;
			_screenshot_directory_creation_successfull = true;
			if (!std::filesystem::exists(screenshot_path.parent_path(), ec))
				if (!(_screenshot_directory_creation_successfull = std::filesystem::create_directories(screenshot_path.parent_path(), ec)))
					LOG(ERROR) << "Failed to create screenshot directory " << screenshot_path.parent_path() << " with error code " << ec.value() << '!';

			// Default to a save failure unless it is reported to succeed below
			bool save_success = false;

			if (auto file = std::ofstream(screenshot_path, std::ios::binary | std::ios::trunc))
			{
				const auto write_callback = [](void *context, void *data, int size) {
					static_cast<std::ofstream *>(context)->write(static_cast<const char *>(data), size);
				};

				switch (_screenshot_format)
				{
				case 0:
					save_success = stbi_write_bmp_to_func(write_callback, &file, _width, _height, comp, pixels.data()) != 0;
					break;
				case 1:
				{
#if 1
					std::vector<uint8_t> encoded_data;
					save_success = fpng::fpng_encode_image_to_memory(pixels.data(), _width, _height, comp, encoded_data);
					write_callback(&file, encoded_data.data(), static_cast<int>(encoded_data.size()));
#else
					save_success = stbi_write_png_to_func(write_callback, &file, _width, _height, comp, pixels.data(), 0) != 0;
#endif
					break;
				}
				case 2:
					save_success = stbi_write_jpg_to_func(write_callback, &file, _width, _height, comp, pixels.data(), _screenshot_jpeg_quality) != 0;
					break;
				}

				if (!file)
					save_success = false;
			}

			if (save_success)
			{
				execute_screenshot_post_save_command(screenshot_path, screenshot_count);

#if RESHADE_FX
				if (include_preset)
				{
					std::filesystem::path screenshot_preset_path = screenshot_path;
					screenshot_preset_path.replace_extension(L".ini");

					// Preset was flushed to disk, so can just copy it over to the new location
					if (!std::filesystem::copy_file(_current_preset_path, screenshot_preset_path, std::filesystem::copy_options::overwrite_existing, ec))
						LOG(ERROR) << "Failed to copy preset file for screenshot to " << screenshot_preset_path << " with error code " << ec.value() << '!';
				}
#endif

#if RESHADE_ADDON
				invoke_addon_event<addon_event::reshade_screenshot>(this, screenshot_path.u8string().c_str());
#endif
			}
			else
			{
				LOG(ERROR) << "Failed to write screenshot to " << screenshot_path << '!';
			}

			if (_last_screenshot_save_successfull)
			{
				_last_screenshot_time = std::chrono::high_resolution_clock::now();
				_last_screenshot_file = screenshot_path;
				_last_screenshot_save_successfull = save_success;
			}
		});
	}
}
bool reshade::runtime::execute_screenshot_post_save_command(const std::filesystem::path &screenshot_path, unsigned int screenshot_count)
{
	if (_screenshot_post_save_command.empty() || _screenshot_post_save_command.extension() != L".exe")
		return false;

	std::string command_line;
	command_line += '\"';
	command_line += _screenshot_post_save_command.u8string();
	command_line += '\"';

	if (!_screenshot_post_save_command_arguments.empty())
	{
		command_line += ' ';
		command_line += expand_macro_string(_screenshot_post_save_command_arguments, {
			{ "AppName", g_target_executable_path.stem().u8string() },
#if RESHADE_FX
			{ "PresetName",  _current_preset_path.stem().u8string() },
#endif
			{ "TargetPath", screenshot_path.u8string() },
			{ "TargetDir", screenshot_path.parent_path().u8string() },
			{ "TargetFileName", screenshot_path.filename().u8string() },
			{ "TargetExt", screenshot_path.extension().u8string() },
			{ "TargetName", screenshot_path.stem().u8string() },
			{ "Count", std::to_string(screenshot_count) }
		});
	}

	if (!utils::execute_command(command_line, g_reshade_base_path / _screenshot_post_save_command_working_directory, _screenshot_post_save_command_no_window))
	{
		LOG(ERROR) << "Failed to execute screenshot post-save command!";
		return false;
	}

	return true;
}

bool reshade::runtime::get_texture_data(api::resource resource, api::resource_usage state, uint8_t *pixels)
{
	const api::resource_desc desc = _device->get_resource_desc(resource);
	const api::format view_format = api::format_to_default_typed(desc.texture.format, 0);

	if (view_format != api::format::r8_unorm &&
		view_format != api::format::r8g8_unorm &&
		view_format != api::format::r8g8b8a8_unorm &&
		view_format != api::format::b8g8r8a8_unorm &&
		view_format != api::format::r8g8b8x8_unorm &&
		view_format != api::format::b8g8r8x8_unorm &&
		view_format != api::format::r10g10b10a2_unorm &&
		view_format != api::format::b10g10r10a2_unorm)
	{
		LOG(ERROR) << "Screenshots are not supported for format " << static_cast<uint32_t>(desc.texture.format) << '!';
		return false;
	}

	// Copy back buffer data into system memory buffer
	api::resource intermediate;
	if (!_device->create_resource(api::resource_desc(desc.texture.width, desc.texture.height, 1, 1, view_format, 1, api::memory_heap::gpu_to_cpu, api::resource_usage::copy_dest), nullptr, api::resource_usage::copy_dest, &intermediate))
	{
		LOG(ERROR) << "Failed to create system memory texture for screenshot capture!";
		return false;
	}

	_device->set_resource_name(intermediate, "ReShade screenshot texture");

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
	cmd_list->barrier(resource, state, api::resource_usage::copy_source);
	cmd_list->copy_texture_region(resource, 0, nullptr, intermediate, 0, nullptr);
	cmd_list->barrier(resource, api::resource_usage::copy_source, state);

	// Wait for any rendering by the application finish before submitting
	// It may have submitted that to a different queue, so simply wait for all to idle here
	_graphics_queue->wait_idle();

	// Copy data from intermediate image into output buffer
	api::subresource_data mapped_data = {};
	if (_device->map_texture_region(intermediate, 0, nullptr, api::map_access::read_only, &mapped_data))
	{
		auto mapped_pixels = static_cast<const uint8_t *>(mapped_data.data);
		const uint32_t pixels_row_pitch = desc.texture.width * 4;

		for (size_t y = 0; y < desc.texture.height; ++y, pixels += pixels_row_pitch, mapped_pixels += mapped_data.row_pitch)
		{
			switch (view_format)
			{
			case api::format::r8_unorm:
				for (size_t x = 0; x < desc.texture.width; ++x)
				{
					pixels[x * 4 + 0] = mapped_pixels[x];
					pixels[x * 4 + 1] = 0;
					pixels[x * 4 + 2] = 0;
					pixels[x * 4 + 3] = 0xFF;
				}
				break;
			case api::format::r8g8_unorm:
				for (size_t x = 0; x < desc.texture.width; ++x)
				{
					pixels[x * 4 + 0] = mapped_pixels[x * 2 + 0];
					pixels[x * 4 + 1] = mapped_pixels[x * 2 + 1];
					pixels[x * 4 + 2] = 0;
					pixels[x * 4 + 3] = 0xFF;
				}
				break;
			case api::format::r8g8b8a8_unorm:
			case api::format::r8g8b8x8_unorm:
				std::memcpy(pixels, mapped_pixels, pixels_row_pitch);
				if (view_format == api::format::r8g8b8x8_unorm)
					for (size_t x = 0; x < pixels_row_pitch; x += 4)
						pixels[x + 3] = 0xFF;
				break;
			case api::format::b8g8r8a8_unorm:
			case api::format::b8g8r8x8_unorm:
				std::memcpy(pixels, mapped_pixels, pixels_row_pitch);
				// Format is BGRA, but output should be RGBA, so flip channels
				for (size_t x = 0; x < pixels_row_pitch; x += 4)
					std::swap(pixels[x + 0], pixels[x + 2]);
				if (view_format == api::format::b8g8r8x8_unorm)
					for (size_t x = 0; x < pixels_row_pitch; x += 4)
						pixels[x + 3] = 0xFF;
				break;
			case api::format::r10g10b10a2_unorm:
			case api::format::b10g10r10a2_unorm:
				for (size_t x = 0; x < pixels_row_pitch; x += 4)
				{
					const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_pixels + x);
					// Divide by 4 to get 10-bit range (0-1023) into 8-bit range (0-255)
					pixels[x + 0] = (( rgba & 0x000003FF)        /  4) & 0xFF;
					pixels[x + 1] = (((rgba & 0x000FFC00) >> 10) /  4) & 0xFF;
					pixels[x + 2] = (((rgba & 0x3FF00000) >> 20) /  4) & 0xFF;
					pixels[x + 3] = (((rgba & 0xC0000000) >> 30) * 85) & 0xFF;
					if (view_format == api::format::b10g10r10a2_unorm)
						std::swap(pixels[x + 0], pixels[x + 2]);
				}
				break;
			}
		}

		_device->unmap_texture_region(intermediate, 0);
	}

	_device->destroy_resource(intermediate);

	return mapped_data.data != nullptr;
}
