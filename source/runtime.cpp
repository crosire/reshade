/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "runtime.hpp"
#include "runtime_internal.hpp"
#include "effect_parser.hpp"
#include "effect_codegen.hpp"
#include "effect_preprocessor.hpp"
#include "version.h"
#include "dll_log.hpp"
#include "dll_resources.hpp"
#include "ini_file.hpp"
#include "addon_manager.hpp"
#include "input.hpp"
#include "input_gamepad.hpp"
#include "com_ptr.hpp"
#include "platform_utils.hpp"
#include "reshade_api_object_impl.hpp"
#include <set>
#include <cmath> // std::abs, std::fmod
#include <cctype> // std::toupper
#include <cwctype> // std::towlower
#include <cstdio> // std::snprintf
#include <cstdlib> // std::malloc, std::rand, std::strtod, std::strtol
#include <cstring> // std::memcpy, std::memset, std::strlen
#include <charconv> // std::to_chars
#include <algorithm> // std::all_of, std::copy_n, std::equal, std::fill_n, std::find, std::find_if, std::for_each, std::max, std::min, std::replace, std::remove, std::remove_if, std::reverse, std::search, std::set_symmetric_difference, std::sort, std::stable_sort, std::swap, std::transform
#include <emmintrin.h>
#include <smmintrin.h>
#include <immintrin.h>
#include <fpng.h>
#include <simple_lossless.h>
#include <stb_image.h>
#include <stb_image_dds.h>
#include <stb_image_write.h>
#include <stb_image_write_hdr_png.h>
#include <stb_image_resize2.h>

bool resolve_path(std::filesystem::path &path, std::error_code &ec)
{
	// First convert path to an absolute path
	// Ignore the working directory and instead start relative paths at the DLL location
	if (path.is_relative())
		path = g_reshade_base_path / path;
	// Finally try to canonicalize the path too
	if (std::filesystem::path canonical_path = std::filesystem::canonical(path, ec); !ec)
		path = std::move(canonical_path);
	else
		path = path.lexically_normal();
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
	return !resolve_path(path, ec) || reshade::ini_file::load_cache(path).has({}, "Techniques");
}

static std::filesystem::path make_relative_path(const std::filesystem::path &path)
{
	if (path.empty())
		return std::filesystem::path();
	// Use ReShade DLL directory as base for relative paths (see 'resolve_path')
	std::filesystem::path proximate_path = path.lexically_proximate(g_reshade_base_path);
	if (proximate_path.wstring().rfind(L"..", 0) != std::wstring::npos)
		return path; // Do not use relative path if preset is in a parent directory
	if (proximate_path.is_relative() && !proximate_path.empty() && proximate_path.native().front() != L'.')
		// Prefix preset path with dot character to better indicate it being a relative path
		proximate_path = L"." / proximate_path;
	return proximate_path;
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

		if (resolve_path(search_path, ec))
		{
			// Append relative file path to absolute search path
			if (std::filesystem::path search_sub_path = search_path / path;
				std::filesystem::exists(search_sub_path, ec))
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
						std::filesystem::exists(search_sub_path, ec))
					{
						path = std::move(search_sub_path);
						return true;
					}
				}
			}
		}
		else
		{
			reshade::log::message(reshade::log::level::warning, "Failed to resolve search path '%s' with error code %d.", search_path.u8string().c_str(), ec.value());
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
			reshade::log::message(reshade::log::level::warning, "Failed to resolve search path '%s' with error code %d.", search_path.u8string().c_str(), ec.value());
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

reshade::runtime::runtime(api::swapchain *swapchain, api::command_queue *graphics_queue, const std::filesystem::path &config_path, bool is_vr) :
	_swapchain(swapchain),
	_device(swapchain->get_device()),
	_graphics_queue(graphics_queue),
	_is_vr(is_vr),
	_start_time(std::chrono::high_resolution_clock::now()),
	_last_present_time(_start_time),
	_last_frame_duration(std::chrono::milliseconds(1)),
	_effect_search_paths({ L".\\" }),
	_texture_search_paths({ L".\\" }),
	_config_path(config_path),
	_screenshot_path(L".\\"),
	_screenshot_name("%AppName% %Date% %Time%_%TimeMS%"), // Include milliseconds by default because users may request more than one screenshot per second
	_screenshot_post_save_command_arguments("\"%TargetPath%\""),
	_screenshot_post_save_command_working_directory(L".\\")
{
	assert(swapchain != nullptr && graphics_queue != nullptr);

	_device->get_property(api::device_properties::vendor_id, &_vendor_id);
	_device->get_property(api::device_properties::device_id, &_device_id);

	_device->get_property(api::device_properties::api_version, &_renderer_id);
	switch (_device->get_api())
	{
	case api::device_api::d3d9:
	case api::device_api::d3d10:
	case api::device_api::d3d11:
	case api::device_api::d3d12:
		break;
	case api::device_api::opengl:
		_renderer_id |= 0x10000;
		break;
	case api::device_api::vulkan:
		_renderer_id |= 0x20000;
		break;
	}

	char device_description[256] = "";
	_device->get_property(api::device_properties::description, device_description);

	if (uint32_t driver_version = 0;
		_device->get_property(api::device_properties::driver_version, &driver_version))
		log::message(log::level::info, "Running on %s Driver %u.%u.", device_description, driver_version / 100, driver_version % 100);
	else
		log::message(log::level::info, "Running on %s.", device_description);

	check_for_update();

	// Default shortcut PrtScrn
	_screenshot_key_data[0] = 0x2C;

#if RESHADE_GUI
	_timestamp_frequency = graphics_queue->get_timestamp_frequency();

	init_gui();
#endif

	// Ensure config path is absolute, in case an add-on created an effect runtime with a relative path
	std::error_code ec;
	resolve_path(_config_path, ec);

	load_config();

	fpng::fpng_init();
}
reshade::runtime::~runtime()
{
	assert(_worker_threads.empty());
	assert(!_is_initialized && _techniques.empty() && _technique_sorting.empty());

#if RESHADE_GUI
	// Save configuration before shutting down to ensure the current window state is written to disk
	save_config();
	ini_file::flush_cache(_config_path);

	deinit_gui();
#endif
}

bool reshade::runtime::on_init()
{
	assert(!_is_initialized);

	const api::resource_desc back_buffer_desc = _device->get_resource_desc(_swapchain->get_back_buffer(0));

	// Avoid initializing on very small swap chains (e.g. implicit swap chain in The Sims 4, which is not used to present in windowed mode)
	if (back_buffer_desc.texture.width <= 16 && back_buffer_desc.texture.height <= 16)
		return false;

	_width = back_buffer_desc.texture.width;
	_height = back_buffer_desc.texture.height;
	_back_buffer_format = api::format_to_default_typed(back_buffer_desc.texture.format);
	_back_buffer_samples = back_buffer_desc.texture.samples;
	_back_buffer_color_space = _swapchain->get_color_space();

	// Create resolve texture and copy pipeline (do this before creating effect resources, to ensure correct back buffer format is set up)
	if (back_buffer_desc.texture.samples > 1 ||
		// Always use resolve texture in OpenGL to flip vertically and support sRGB + binding effect stencil
		(_device->get_api() == api::device_api::opengl && !_is_vr) ||
		// Some effects rely on there being an alpha channel available, so create resolve texture if that is not the case
		(_back_buffer_format == api::format::r8g8b8x8_unorm || _back_buffer_format == api::format::b8g8r8x8_unorm))
	{
		switch (_back_buffer_format)
		{
		case api::format::r8g8b8x8_unorm:
			_back_buffer_format = api::format::r8g8b8a8_unorm;
			break;
		case api::format::b8g8r8x8_unorm:
			_back_buffer_format = api::format::b8g8r8a8_unorm;
			break;
		}

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
			log::message(log::level::error, "Failed to create resolve texture resource!");
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
				log::message(log::level::error, "Failed to create resolve shader resource view!");
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
				log::message(log::level::error, "Failed to create copy pipeline!");
				goto exit_failure;
			}
		}
	}

	// Create an empty texture, which is bound to shader resource view slots with an unknown semantic (since it is not valid to bind a zero handle in Vulkan, unless the 'VK_EXT_robustness2' extension is enabled)
	if (_empty_tex == 0)
	{
		// Use VK_FORMAT_R16_SFLOAT format, since it is mandatory according to the spec (see https://www.khronos.org/registry/vulkan/specs/1.1/html/vkspec.html#features-required-format-support)
		if (!_device->create_resource(
				api::resource_desc(1, 1, 1, 1, api::format::r16_float, 1, api::memory_heap::gpu_only, api::resource_usage::shader_resource),
				nullptr, api::resource_usage::shader_resource, &_empty_tex))
		{
			log::message(log::level::error, "Failed to create empty texture resource!");
			goto exit_failure;
		}

		_device->set_resource_name(_empty_tex, "ReShade empty texture");

		if (!_device->create_resource_view(_empty_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format::r16_float), &_empty_srv))
		{
			log::message(log::level::error, "Failed to create empty texture shader resource view!");
			goto exit_failure;
		}
	}

	// Create effect color and stencil resource
	{
		api::format stencil_format = api::format::unknown;

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
				stencil_format = format;
				break;
			}
		}

		if (add_effect_permutation(_width, _height, _back_buffer_format, stencil_format, _back_buffer_color_space) != 0)
			goto exit_failure;
	}

	// Create render targets for the back buffer resources
	for (uint32_t i = 0, count = _swapchain->get_back_buffer_count(); i < count; ++i)
	{
		const api::resource back_buffer_resource = _swapchain->get_back_buffer(i);

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
			log::message(log::level::error, "Failed to create back buffer render targets!");
			goto exit_failure;
		}
	}

	create_state_block(_device, &_app_state);

#if RESHADE_GUI
	if (!init_imgui_resources())
		goto exit_failure;

	if (_is_vr && !init_gui_vr())
		goto exit_failure;
#endif

	{
		const input::window_handle window = get_hwnd();
		if (window != nullptr && !_is_vr)
		{
			_input = input::register_window(window);
			_primary_input_handler = _input.use_count() == 1;
		}
		else
		{
			_input.reset();
			_primary_input_handler = _input_gamepad != nullptr;
		}

		// GTK 3 enables transparency for windows, which messes with effects that do not return an alpha value, so disable that again
		if (window != nullptr)
			utils::set_window_transparency(window, false);
	}

	// Reset frame count to zero so effects are loaded in 'update_effects'
	_frame_count = 0;

	_is_initialized = true;
	_last_reload_time = std::chrono::high_resolution_clock::now(); // Intentionally set to current time, so that duration to last reload is valid even when there is no reload on init

	_preset_save_successful = true;
	_last_screenshot_save_successful = true;

#if RESHADE_ADDON
	invoke_addon_event<addon_event::init_effect_runtime>(this);
#endif

	log::message(log::level::info, "Recreated runtime environment on runtime %p ('%s').", this, _config_path.u8string().c_str());

	return true;

exit_failure:
	_device->destroy_resource(_empty_tex);
	_empty_tex = {};
	_device->destroy_resource_view(_empty_srv);
	_empty_srv = {};

	for (const effect_permutation &permutation : _effect_permutations)
	{
		_device->destroy_resource(permutation.color_tex);
		_device->destroy_resource_view(permutation.color_srv[0]);
		_device->destroy_resource_view(permutation.color_srv[1]);

		_device->destroy_resource(permutation.stencil_tex);
		_device->destroy_resource_view(permutation.stencil_dsv);
	}
	_effect_permutations.clear();

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

	destroy_state_block(_device, _app_state);
	_app_state = {};

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

	// Already performs a wait for idle, so no need to do it again before destroying resources below
	destroy_effects();

	_device->destroy_resource(_empty_tex);
	_empty_tex = {};
	_device->destroy_resource_view(_empty_srv);
	_empty_srv = {};

	for (const effect_permutation &permutation : _effect_permutations)
	{
		_device->destroy_resource(permutation.color_tex);
		_device->destroy_resource_view(permutation.color_srv[0]);
		_device->destroy_resource_view(permutation.color_srv[1]);

		_device->destroy_resource(permutation.stencil_tex);
		_device->destroy_resource_view(permutation.stencil_dsv);
	}
	_effect_permutations.clear();

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

	destroy_state_block(_device, _app_state);
	_app_state = {};

	_width = _height = 0;
	_back_buffer_format = api::format::unknown;
	_back_buffer_samples = 1;
	_back_buffer_color_space = api::color_space::unknown;

#if RESHADE_GUI
	if (_is_vr)
		deinit_gui_vr();

	destroy_imgui_resources();
#endif

#if RESHADE_ADDON
	invoke_addon_event<addon_event::destroy_effect_runtime>(this);
#endif

	log::message(log::level::info, "Destroyed runtime environment on runtime %p ('%s').", this, _config_path.u8string().c_str());
}
void reshade::runtime::on_present()
{
	if (!_is_initialized)
		return;

#if RESHADE_ADDON
	_is_in_present_call = true;
#endif

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();

	capture_state(cmd_list, _app_state);

	uint32_t back_buffer_index = (_back_buffer_resolved != 0 ? 2 : 0) + _swapchain->get_current_back_buffer_index() * 2;
	const api::resource back_buffer_resource = _device->get_resource_from_view(_back_buffer_targets[back_buffer_index]);

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

	// Lock input so it cannot be modified by other threads while we are reading it here
	std::unique_lock<std::recursive_mutex> input_lock;
	if (_input != nullptr)
		input_lock = _input->lock();

	update_effects();

	_current_time = std::chrono::system_clock::now();

	if (_should_save_screenshot && _screenshot_save_before && _effects_enabled && !_effects_rendered_this_frame)
		save_screenshot("Before");

	if (!is_loading() && !_techniques.empty())
	{
		if (_back_buffer_resolved != 0)
		{
			runtime::render_effects(cmd_list, _back_buffer_targets[0], _back_buffer_targets[1]);
		}
		else
		{
			cmd_list->barrier(back_buffer_resource, api::resource_usage::present, api::resource_usage::render_target);
			runtime::render_effects(cmd_list, _back_buffer_targets[back_buffer_index], _back_buffer_targets[back_buffer_index + 1]);
			cmd_list->barrier(back_buffer_resource, api::resource_usage::render_target, api::resource_usage::present);
		}
	}

	if (_should_save_screenshot)
		save_screenshot(_screenshot_save_before ? "After" : nullptr);

	_frame_count++;
	const auto current_time = std::chrono::high_resolution_clock::now();
	_last_frame_duration = current_time - _last_present_time; _last_present_time = current_time;

#if RESHADE_GUI
	// Draw overlay
	if (_is_vr)
		draw_gui_vr();
	else
		draw_gui();

	if (_should_save_screenshot && _screenshot_save_gui && (_show_overlay || (_preview_texture != std::numeric_limits<size_t>::max() && _effects_enabled)))
		save_screenshot("Overlay");

	_block_input_next_frame = false;
#endif

	// All screenshots were created at this point, so reset request
	_should_save_screenshot = false;

	// Handle keyboard shortcuts
	if (!_ignore_shortcuts && _input != nullptr)
	{
		if (_input->is_key_pressed(_effects_key_data, _force_shortcut_modifiers))
		{
#if RESHADE_ADDON
			if (!invoke_addon_event<addon_event::reshade_set_effects_state>(this, !_effects_enabled))
#endif
				_effects_enabled = !_effects_enabled;
		}

		if (_input->is_key_pressed(_screenshot_key_data, _force_shortcut_modifiers))
		{
			_screenshot_count++;
			_should_save_screenshot = true; // Remember that we want to save a screenshot next frame
		}

		// Do not allow the following shortcuts while effects are being loaded or initialized (since they affect that state)
		if (!is_loading())
		{
			if (_effects_enabled && !_is_in_preset_transition)
			{
				for (effect &effect : _effects)
				{
					if (!effect.rendering)
						continue;

					for (uniform &variable : effect.uniforms)
					{
						if (_input->is_key_pressed(variable.toggle_key_data, _force_shortcut_modifiers))
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
								}
								break;
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
								}
								break;
							}

#if RESHADE_GUI
							if (_auto_save_preset)
								save_current_preset();
							else
								_preset_is_modified = true;
#endif
						}
					}
				}

				for (technique &tech : _techniques)
				{
					if (_input->is_key_pressed(tech.toggle_key_data, _force_shortcut_modifiers))
					{
						if (!tech.enabled)
							enable_technique(tech);
						else
							disable_technique(tech);

#if RESHADE_GUI
						if (_auto_save_preset)
							save_current_preset();
						else
							_preset_is_modified = true;
#endif
					}
				}
			}

			if (_input->is_key_pressed(_reload_key_data, _force_shortcut_modifiers))
				reload_effects();

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
			if (_is_in_preset_transition)
				load_current_preset();
		}
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

			cmd_list->push_descriptors(api::shader_stage::pixel, _copy_pipeline_layout, 0, api::descriptor_table_update { {}, 0, 0, 1, api::descriptor_type::sampler, &_copy_sampler_state });
			cmd_list->push_descriptors(api::shader_stage::pixel, _copy_pipeline_layout, 1, api::descriptor_table_update { {}, 0, 0, 1, api::descriptor_type::shader_resource_view, &_back_buffer_resolved_srv });

			const api::viewport viewport = { 0.0f, 0.0f, static_cast<float>(_width), static_cast<float>(_height), 0.0f, 1.0f };
			cmd_list->bind_viewports(0, 1, &viewport);
			const api::rect scissor_rect = { 0, 0, static_cast<int32_t>(_width), static_cast<int32_t>(_height) };
			cmd_list->bind_scissor_rects(0, 1, &scissor_rect);

			const bool srgb_write_enable = (_back_buffer_format == api::format::r8g8b8a8_unorm_srgb || _back_buffer_format == api::format::b8g8r8a8_unorm_srgb);
			cmd_list->bind_render_targets_and_depth_stencil(1, &_back_buffer_targets[back_buffer_index + srgb_write_enable]);

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

	// Apply previous state from application
	apply_state(cmd_list, _app_state);

#if RESHADE_ADDON
	invoke_addon_event<addon_event::reshade_present>(this);

	_is_in_present_call = false;
#endif
	_effects_rendered_this_frame = false;

	// Update input status
	if (_primary_input_handler && _input != nullptr)
		_input->next_frame();
	if (_primary_input_handler && _input_gamepad != nullptr)
		_input_gamepad->next_frame();

	// Save modified INI files
	if (!ini_file::flush_cache())
		_preset_save_successful = false;

#if RESHADE_ADDON == 1
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

		if (addon_enabled != was_enabled)
		{
			if (was_enabled)
				_backup_texture_semantic_bindings = _texture_semantic_bindings;

			for (const auto &binding : _backup_texture_semantic_bindings)
			{
				if (binding.second.first == _effect_permutations[0].color_srv[0] && binding.second.second == _effect_permutations[0].color_srv[1])
					continue;

				update_texture_bindings(binding.first.c_str(), addon_enabled ? binding.second.first : api::resource_view { 0 }, addon_enabled ? binding.second.second : api::resource_view { 0 });
			}
		}
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

	const auto config_get = [&config](const std::string &section, const std::string &key, auto &values) {
		if (config.get(section, key, values))
			return true;
		// Fall back to global configuration when an entry does not exist in the local configuration
		return global_config().get(section, key, values);
	};

	config_get("INPUT", "ForceShortcutModifiers", _force_shortcut_modifiers);
	config_get("INPUT", "KeyScreenshot", _screenshot_key_data);
	config_get("INPUT", "KeyEffects", _effects_key_data);
	config_get("INPUT", "KeyNextPreset", _next_preset_key_data);
	config_get("INPUT", "KeyPreviousPreset", _prev_preset_key_data);
	config_get("INPUT", "KeyReload", _reload_key_data);

	config_get("GENERAL", "NoDebugInfo", _no_debug_info);
	config_get("GENERAL", "NoEffectCache", _no_effect_cache);
	config_get("GENERAL", "NoReloadOnInit", _no_reload_on_init);

	config_get("GENERAL", "EffectSearchPaths", _effect_search_paths);
	config_get("GENERAL", "PerformanceMode", _performance_mode);
	config_get("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
	config_get("GENERAL", "SkipLoadingDisabledEffects", _effect_load_skipping);
	config_get("GENERAL", "TextureSearchPaths", _texture_search_paths);
	config_get("GENERAL", "IntermediateCachePath", _effect_cache_path);

	config_get("GENERAL", "StartupPresetPath", _startup_preset_path);
	config_get("GENERAL", "PresetPath", _current_preset_path);
	config_get("GENERAL", "PresetTransitionDuration", _preset_transition_duration);

	// Fall back to temp directory if cache path does not exist
	std::error_code ec;
	if (_effect_cache_path.empty() || !resolve_path(_effect_cache_path, ec))
	{
		_effect_cache_path = std::filesystem::temp_directory_path(ec) / "ReShade";
		std::filesystem::create_directory(_effect_cache_path, ec);
		if (ec)
			log::message(log::level::error, "Failed to create effect cache directory '%s' with error code %d!", _effect_cache_path.u8string().c_str(), ec.value());
	}

	// Use startup preset instead of last selection
	if (!_startup_preset_path.empty() && resolve_preset_path(_startup_preset_path, ec))
		_current_preset_path = _startup_preset_path;
	// Use default if the preset file does not exist yet
	else if (!resolve_preset_path(_current_preset_path, ec))
		_current_preset_path = g_reshade_base_path / L"ReShadePreset.ini";

	std::vector<unsigned int> preset_key_data;
	std::vector<std::filesystem::path> preset_shortcut_paths;
	config_get("GENERAL", "PresetShortcutKeys", preset_key_data);
	config_get("GENERAL", "PresetShortcutPaths", preset_shortcut_paths);

	_preset_shortcuts.clear();
	for (size_t i = 0; i < preset_shortcut_paths.size() && (i * 4 + 4) <= preset_key_data.size(); ++i)
	{
		preset_shortcut shortcut;
		shortcut.preset_path = preset_shortcut_paths[i];
		std::copy_n(&preset_key_data[i * 4], 4, shortcut.key_data);
		_preset_shortcuts.push_back(std::move(shortcut));
	}

	config_get("SCREENSHOT", "SavePath", _screenshot_path);
	config_get("SCREENSHOT", "SoundPath", _screenshot_sound_path);
	config_get("SCREENSHOT", "ClearAlpha", _screenshot_clear_alpha);
	config_get("SCREENSHOT", "FileFormat", _screenshot_format);
	config_get("SCREENSHOT", "FileNaming", _screenshot_name);
	config_get("SCREENSHOT", "JPEGQuality", _screenshot_jpeg_quality);
	config_get("SCREENSHOT", "SaveBeforeShot", _screenshot_save_before);
	config_get("SCREENSHOT", "SavePresetFile", _screenshot_include_preset);
#if RESHADE_GUI
	config_get("SCREENSHOT", "SaveOverlayShot", _screenshot_save_gui);
#endif
	config_get("SCREENSHOT", "PostSaveCommand", _screenshot_post_save_command);
	config_get("SCREENSHOT", "PostSaveCommandArguments", _screenshot_post_save_command_arguments);
	config_get("SCREENSHOT", "PostSaveCommandWorkingDirectory", _screenshot_post_save_command_working_directory);
	config_get("SCREENSHOT", "PostSaveCommandHideWindow", _screenshot_post_save_command_hide_window);

#if RESHADE_GUI
	load_config_gui(config);
#endif
}
void reshade::runtime::save_config() const
{
	ini_file &config = ini_file::load_cache(_config_path);

	config.set("INPUT", "ForceShortcutModifiers", _force_shortcut_modifiers);
	config.set("INPUT", "KeyScreenshot", _screenshot_key_data);
	config.set("INPUT", "KeyEffects", _effects_key_data);
	config.set("INPUT", "KeyNextPreset", _next_preset_key_data);
	config.set("INPUT", "KeyPreviousPreset", _prev_preset_key_data);
	config.set("INPUT", "KeyReload", _reload_key_data);

	config.set("GENERAL", "NoDebugInfo", _no_debug_info);
	config.set("GENERAL", "NoEffectCache", _no_effect_cache);
	config.set("GENERAL", "NoReloadOnInit", _no_reload_on_init);

	config.set("GENERAL", "EffectSearchPaths", _effect_search_paths);
	config.set("GENERAL", "PerformanceMode", _performance_mode);
	config.set("GENERAL", "PreprocessorDefinitions", _global_preprocessor_definitions);
	config.set("GENERAL", "SkipLoadingDisabledEffects", _effect_load_skipping);
	config.set("GENERAL", "TextureSearchPaths", _texture_search_paths);
	config.set("GENERAL", "IntermediateCachePath", _effect_cache_path);

	config.set("GENERAL", "StartupPresetPath", make_relative_path(_startup_preset_path));
	config.set("GENERAL", "PresetPath", make_relative_path(_current_preset_path));

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

	config.set("SCREENSHOT", "SavePath", _screenshot_path);
	config.set("SCREENSHOT", "SoundPath", _screenshot_sound_path);
	config.set("SCREENSHOT", "ClearAlpha", _screenshot_clear_alpha);
	config.set("SCREENSHOT", "FileFormat", _screenshot_format);
	config.set("SCREENSHOT", "FileNaming", _screenshot_name);
	config.set("SCREENSHOT", "JPEGQuality", _screenshot_jpeg_quality);
	config.set("SCREENSHOT", "SaveBeforeShot", _screenshot_save_before);
	config.set("SCREENSHOT", "SavePresetFile", _screenshot_include_preset);
#if RESHADE_GUI
	config.set("SCREENSHOT", "SaveOverlayShot", _screenshot_save_gui);
#endif
	config.set("SCREENSHOT", "PostSaveCommand", _screenshot_post_save_command);
	config.set("SCREENSHOT", "PostSaveCommandArguments", _screenshot_post_save_command_arguments);
	config.set("SCREENSHOT", "PostSaveCommandWorkingDirectory", _screenshot_post_save_command_working_directory);
	config.set("SCREENSHOT", "PostSaveCommandHideWindow", _screenshot_post_save_command_hide_window);

#if RESHADE_GUI
	save_config_gui(config);
#endif
}

void reshade::runtime::load_current_preset()
{
	_preset_is_incomplete = false;
	_preset_save_successful = true;

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
	if (_reload_remaining_effects != 0 && (!_is_in_preset_transition || _last_preset_switching_time == _last_present_time)) // ... unless this is the 'load_current_preset' call in 'update_effects' or the call every frame during preset transition
	{
		if (_performance_mode || preset_preprocessor_definitions != _preset_preprocessor_definitions)
		{
			_preset_preprocessor_definitions = std::move(preset_preprocessor_definitions);
			reload_effects();
			return; // Preset values are loaded in 'update_effects' during effect loading
		}

		if (std::find_if(technique_list.cbegin(), technique_list.cend(),
				[this](const std::string_view technique_name) {
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

	for (const std::string_view technique_name : technique_list)
	{
		if (std::find_if(_techniques.begin(), _techniques.end(),
				[name = technique_name.substr(0, technique_name.find('@'))](const technique &technique) {
					return technique.name == name;
				}) == _techniques.end())
		{
			if (_reload_remaining_effects == 0)
				log::message(log::level::warning, "Preset '%s' uses unknown technique '%*s'.", _current_preset_path.u8string().c_str(), technique_name.size(), technique_name.data());
			_preset_is_incomplete = true;
		}
	}

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

	if (_is_in_preset_transition && transition_ms_left <= 0)
		_is_in_preset_transition = false;

	for (effect &effect : _effects)
	{
		const std::string effect_name = effect.source_file.filename().u8string();

		for (uniform &variable : effect.uniforms)
		{
			if (variable.special != special_uniform::none ||
				variable.annotation_as_uint("nosave"))
				continue;

			if (variable.supports_toggle_key())
			{
				if (!preset.get(effect_name, "Key" + variable.name, variable.toggle_key_data))
					std::memset(variable.toggle_key_data, 0, sizeof(variable.toggle_key_data));
			}

			// Reset values to defaults before loading from a new preset
			if (!_is_in_preset_transition)
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
				if (_is_in_preset_transition)
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
			std::memset(tech.toggle_key_data, 0, sizeof(tech.toggle_key_data));
	}

	// Reverse queue so that effects are enabled in the order they are defined in the preset (since the queue is worked from back to front)
	std::reverse(_reload_create_queue.begin(), _reload_create_queue.end());
}
void reshade::runtime::save_current_preset(ini_file &preset) const
{
	assert(!_is_in_preset_transition);

	// Build list of active techniques and effects
	std::set<size_t> effect_list;
	std::vector<std::string> technique_list;
	technique_list.reserve(_techniques.size());
	std::vector<std::string> sorted_technique_list;
	sorted_technique_list.reserve(_technique_sorting.size());

	for (size_t technique_index : _technique_sorting)
	{
		const technique &tech = _techniques[technique_index];

		if (tech.annotation_as_uint("nosave"))
			continue;

		const std::string unique_name = tech.name + '@' + _effects[tech.effect_index].source_file.filename().u8string();

		if (tech.enabled)
			technique_list.push_back(unique_name);
		if (tech.enabled || tech.toggle_key_data[0] != 0)
			effect_list.insert(tech.effect_index);

		// Keep track of the order of all techniques and not just the enabled ones
		sorted_technique_list.push_back(unique_name);

		if (tech.toggle_key_data[0] != 0)
			preset.set({}, "Key" + unique_name, tech.toggle_key_data);
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
			if (variable.special != special_uniform::none ||
				variable.annotation_as_uint("nosave"))
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
	std::wstring filter_text;

	resolve_path(filter_path, ec);

	if (const std::filesystem::file_type file_type = std::filesystem::status(filter_path, ec).type();
		file_type != std::filesystem::file_type::directory)
	{
		if (file_type == std::filesystem::file_type::not_found)
		{
			filter_text = filter_path.filename().wstring();
			if (!filter_text.empty())
				filter_path = filter_path.parent_path().wstring();
		}
		else
		{
			_current_preset_path = filter_path;
			_last_preset_switching_time = _last_present_time;
			_is_in_preset_transition = true;

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

		const std::wstring preset_name = preset_path.stem().wstring();
		// Only add those files that are matching the filter text
		if (filter_text.empty() ||
			std::search(preset_name.cbegin(), preset_name.cend(), filter_text.begin(), filter_text.end(),
				[](auto c1, auto c2) { return std::towlower(c1) == std::towlower(c2); }) != preset_name.cend())
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
	_is_in_preset_transition = true;

	return true;
}

bool reshade::runtime::load_effect(const std::filesystem::path &source_file, const ini_file &preset, size_t effect_index, size_t permutation_index, bool force_load, bool preprocess_required)
{
	const std::chrono::high_resolution_clock::time_point time_load_started = std::chrono::high_resolution_clock::now();

	// Generate a unique string identifying this effect
	std::string attributes;
	attributes += "app=" + g_target_executable_path.stem().u8string() + ';';
	attributes += "width=" + std::to_string(_effect_permutations[permutation_index].width) + ';';
	attributes += "height=" + std::to_string(_effect_permutations[permutation_index].height) + ';';
	attributes += "color_space=" + std::to_string(static_cast<uint32_t>(_effect_permutations[permutation_index].color_space)) + ';';
	attributes += "color_format=" + std::to_string(static_cast<uint32_t>(_effect_permutations[permutation_index].color_format)) + ';';
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

#if RESHADE_ADDON
	std::vector<std::string> addon_definitions;
	addon_definitions.reserve(addon_loaded_info.size());
	for (const addon_info &info : addon_loaded_info)
	{
		if (info.handle == nullptr)
			continue; // Skip disabled add-ons

		std::string addon_definition;
		addon_definition.reserve(6 + info.name.size());
		addon_definition = "ADDON_";
		std::transform(info.name.begin(), info.name.end(), std::back_inserter(addon_definition),
			[](const std::string::value_type c) {
				return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ? c : (c >= 'a' && c <= 'z') ? static_cast<std::string::value_type>(c - 'a' + 'A') : '_';
			});
		preprocessor_definitions.emplace_back(addon_definition, std::to_string(std::max(1, info.version.number.major * 10000 + info.version.number.minor * 100 + info.version.number.build)));
	}
#endif

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
	if (permutation_index == 0 && (source_file != effect.source_file || source_hash != effect.source_hash))
	{
		if (effect.created)
		{
			if (_reload_remaining_effects != std::numeric_limits<size_t>::max())
				_reload_remaining_effects--;
			return false; // Cannot reset an effect that has not been destroyed
		}

		// Source hash has changed, reset effect and load from scratch, rather than updating
		effect = {
			source_file,
			source_hash,
			source_file.extension() == L".addonfx"
		};

		// Allocate the default permutation
		effect.permutations.resize(1);
	}

	if (_effect_load_skipping && !force_load)
	{
		if (std::vector<std::string> techniques;
			preset.get({}, "Techniques", techniques) && !techniques.empty())
		{
			effect.skipped = std::find_if(techniques.cbegin(), techniques.cend(),
				[&effect_name](const std::string &technique) {
					const size_t at_pos = technique.find('@') + 1;
					return at_pos == 0 || technique.find(effect_name, at_pos) == at_pos;
				}) == techniques.cend();

			if (effect.skipped)
			{
				if (_reload_remaining_effects != std::numeric_limits<size_t>::max())
					_reload_remaining_effects--;
				return false;
			}
		}
	}

	assert(permutation_index < effect.permutations.size());
	effect::permutation &permutation = effect.permutations[permutation_index];

	bool preprocessed = effect.preprocessed && permutation_index == 0;
	bool compiled = effect.compiled && permutation_index == 0;
	bool source_cached = false;
	std::string source;
	std::string errors;

	if (!preprocessed && (preprocess_required || (source_cached = load_effect_cache(source_file.stem().u8string() + '-' + std::to_string(_renderer_id) + '-' + std::to_string(source_hash), "i", source)) == false))
	{
		reshadefx::preprocessor pp;
		pp.add_macro_definition("__RESHADE__", std::to_string(VERSION_MAJOR * 10000 + VERSION_MINOR * 100 + VERSION_REVISION));
		pp.add_macro_definition("__RESHADE_PERMUTATION__", permutation_index != 0 ? "1" : "0");
		pp.add_macro_definition("__RESHADE_PERFORMANCE_MODE__", _performance_mode ? "1" : "0");
		pp.add_macro_definition("__VENDOR__", std::to_string(_vendor_id));
		pp.add_macro_definition("__DEVICE__", std::to_string(_device_id));
		pp.add_macro_definition("__RENDERER__", std::to_string(_renderer_id));
		pp.add_macro_definition("__APPLICATION__", std::to_string( // Truncate hash to 32-bit, since lexer currently only supports 32-bit numbers anyway
			std::hash<std::string>()(g_target_executable_path.stem().u8string()) & 0xFFFFFFFF));
		pp.add_macro_definition("BUFFER_WIDTH", std::to_string(_effect_permutations[permutation_index].width));
		pp.add_macro_definition("BUFFER_HEIGHT", std::to_string(_effect_permutations[permutation_index].height));
		pp.add_macro_definition("BUFFER_RCP_WIDTH", "(1.0 / BUFFER_WIDTH)");
		pp.add_macro_definition("BUFFER_RCP_HEIGHT", "(1.0 / BUFFER_HEIGHT)");
		pp.add_macro_definition("BUFFER_COLOR_SPACE", std::to_string(static_cast<uint32_t>(_effect_permutations[permutation_index].color_space)));
		pp.add_macro_definition("BUFFER_COLOR_FORMAT", std::to_string(static_cast<uint32_t>(_effect_permutations[permutation_index].color_format)));
		pp.add_macro_definition("BUFFER_COLOR_BIT_DEPTH", std::to_string(api::format_bit_depth(_effect_permutations[permutation_index].color_format)));

		for (const std::pair<std::string, std::string> &definition : preprocessor_definitions)
		{
			if (definition.first.empty())
				continue; // Skip invalid definitions

			pp.add_macro_definition(definition.first, definition.second.empty() ? "1" : definition.second);
		}
		preprocessor_definitions.clear(); // Clear before reusing for used preprocessor definitions below

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
		preprocessed = pp.append_file(source_file);

		// Append preprocessor errors to the error list
		errors += pp.errors();

		if (preprocessed)
		{
			source = pp.output();

			// Keep track of used preprocessor definitions (so they can be displayed in the overlay)
			for (const std::pair<std::string, std::string> &definition : pp.used_macro_definitions())
			{
				if (definition.first.size() < 8 ||
					definition.first[0] == '_' ||
					definition.first.compare(0, 7, "BUFFER_") == 0 ||
					definition.first.compare(0, 8, "RESHADE_") == 0 ||
					definition.first.find("INCLUDE_") != std::string::npos)
					continue;

				preprocessor_definitions.emplace_back(definition.first, trim(definition.second));

				// Write used preprocessor definitions to the cached source
				source = "// " + definition.first + '=' + definition.second + '\n' + source;
			}

			source_cached = save_effect_cache(source_file.stem().u8string() + '-' + std::to_string(_renderer_id) + '-' + std::to_string(source_hash), "i", source);
		}

		if (permutation_index == 0)
		{
			effect.definitions = std::move(preprocessor_definitions);
			std::sort(effect.definitions.begin(), effect.definitions.end());

			// Keep track of included files
			effect.included_files = pp.included_files();
			std::sort(effect.included_files.begin(), effect.included_files.end()); // Sort file names alphabetically

			effect.preprocessed = preprocessed;
		}
	}
	else
	{
		if (permutation_index == 0 && !source.empty())
		{
			effect.definitions.clear();

			// Read used preprocessor definitions from the cached source
			for (size_t offset = 0, next; source.compare(offset, 3, "// ") == 0; offset = next + 1)
			{
				offset += 3;
				next = source.find('\n', offset);
				if (next == std::string::npos)
					break;

				if (const size_t equals_index = source.find('=', offset);
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

	std::unique_ptr<reshadefx::codegen> codegen;
	size_t spec_constants_hash = 0;
	if (!compiled && !source.empty())
	{
		unsigned shader_model;
		if (_renderer_id == 0x9000)
			shader_model = 30; // D3D9
		else if (_renderer_id < 0xa100)
			shader_model = 40; // D3D10 (including feature level 9)
		else if (_renderer_id < 0xb000 || _device->get_api() == api::device_api::d3d10)
			shader_model = 41; // D3D10.1
		else if (_renderer_id < 0xc000 || _device->get_api() == api::device_api::d3d11)
			shader_model = 50; // D3D11
		else
			shader_model = 51; // D3D12

		if ((_renderer_id & 0xF0000) == 0)
			codegen.reset(reshadefx::create_codegen_dxbc(shader_model, !_no_debug_info, _performance_mode, _performance_mode ? 3 : 1));
		else if (_renderer_id < 0x20000)
			codegen.reset(reshadefx::create_codegen_glsl(false, !_no_debug_info, _performance_mode, false, true));
		else // Vulkan uses SPIR-V input
			codegen.reset(reshadefx::create_codegen_spirv(true, !_no_debug_info, _performance_mode, false, false));

		reshadefx::parser parser;

		// Compile the pre-processed source code (try the compile even if the preprocessor step failed to get additional error information)
		compiled = parser.parse(std::move(source), codegen.get());

		// Append parser errors to the error list
		errors  += parser.errors();

		// Write result to effect module
		permutation.module = codegen->module();

		if (compiled)
		{
			if (permutation_index == 0)
			{
				effect.uniforms.clear();

				// Create space for all variables (aligned to 16 bytes)
				effect.uniform_data_storage.resize((permutation.module.total_uniform_size + 15) & ~15);

				for (uniform variable : permutation.module.uniforms)
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
			}
			else
			{
				if (permutation.module.total_uniform_size != effect.permutations[0].module.total_uniform_size ||
					!std::equal(
						permutation.module.uniforms.begin(), permutation.module.uniforms.end(),
						effect.permutations[0].module.uniforms.begin(), effect.permutations[0].module.uniforms.end(),
						[](const reshadefx::uniform &lhs_variable, const reshadefx::uniform &rhs_variable) {
							return lhs_variable.offset == rhs_variable.offset && lhs_variable.size == rhs_variable.size && lhs_variable.type == rhs_variable.type && lhs_variable.name == rhs_variable.name;
						}))
				{
					errors += "error: effect permutation defines different uniform variables";

					std::vector<std::string> lhs_uniform_names;
					lhs_uniform_names.reserve(permutation.module.uniforms.size());
					std::transform(
						permutation.module.uniforms.begin(), permutation.module.uniforms.end(),
						std::back_inserter(lhs_uniform_names),
						[](const reshadefx::uniform &variable) { return variable.name; });
					std::sort(lhs_uniform_names.begin(), lhs_uniform_names.end());

					std::vector<std::string> rhs_uniform_names;
					rhs_uniform_names.reserve(effect.permutations[0].module.uniforms.size());
					std::transform(
						effect.permutations[0].module.uniforms.begin(), effect.permutations[0].module.uniforms.end(),
						std::back_inserter(rhs_uniform_names),
						[](const reshadefx::uniform &variable) { return variable.name; });
					std::sort(rhs_uniform_names.begin(), rhs_uniform_names.end());

					std::vector<std::string> different_uniform_names;
					different_uniform_names.reserve(std::max(lhs_uniform_names.size(), rhs_uniform_names.size()));
					std::set_symmetric_difference(
						lhs_uniform_names.begin(), lhs_uniform_names.end(),
						rhs_uniform_names.begin(), rhs_uniform_names.end(),
						std::back_inserter(different_uniform_names));

					if (!different_uniform_names.empty())
					{
						errors += " (";
						errors += different_uniform_names[0];
						for (size_t i = 1; i < different_uniform_names.size(); ++i)
							errors += ", " + different_uniform_names[i];
						errors +=  ')';
					}

					errors += '\n';
					compiled = false;
				}

				if (!std::equal(
						permutation.module.techniques.begin(), permutation.module.techniques.end(),
						effect.permutations[0].module.techniques.begin(), effect.permutations[0].module.techniques.end(),
						[](const reshadefx::technique &lhs_tech, const reshadefx::technique &rhs_tech) {
							return lhs_tech.name == rhs_tech.name;
						}))
				{
					errors += "error: effect permutation defines different techniques";

					std::vector<std::string> lhs_technique_names;
					lhs_technique_names.reserve(permutation.module.techniques.size());
					std::transform(
						permutation.module.techniques.begin(), permutation.module.techniques.end(),
						std::back_inserter(lhs_technique_names),
						[](const reshadefx::technique &tech) { return tech.name; });
					std::sort(lhs_technique_names.begin(), lhs_technique_names.end());

					std::vector<std::string> rhs_technique_names;
					rhs_technique_names.reserve(effect.permutations[0].module.techniques.size());
					std::transform(
						effect.permutations[0].module.techniques.begin(), effect.permutations[0].module.techniques.end(),
						std::back_inserter(rhs_technique_names),
						[](const reshadefx::technique &tech) { return tech.name; });
					std::sort(rhs_technique_names.begin(), rhs_technique_names.end());

					std::vector<std::string> different_technique_names;
					different_technique_names.reserve(std::max(lhs_technique_names.size(), rhs_technique_names.size()));
					std::set_symmetric_difference(
						lhs_technique_names.begin(), lhs_technique_names.end(),
						rhs_technique_names.begin(), rhs_technique_names.end(),
						std::back_inserter(different_technique_names));

					if (!different_technique_names.empty())
					{
						errors += " (";
						errors += different_technique_names[0];
						for (size_t i = 1; i < different_technique_names.size(); ++i)
							errors += ", " + different_technique_names[i];
						errors += ')';
					}

					errors += '\n';
					compiled = false;
				}
			}

			// Fill all specialization constants with values from the current preset
			if (_performance_mode)
			{
				std::string spec_constant_attributes;

				for (reshadefx::uniform &spec_constant : permutation.module.spec_constants)
				{
					switch (spec_constant.type.base)
					{
					case reshadefx::type::t_int:
						preset.get(effect_name, spec_constant.name, spec_constant.initializer_value.as_int);
						break;
					case reshadefx::type::t_bool:
					case reshadefx::type::t_uint:
						preset.get(effect_name, spec_constant.name, spec_constant.initializer_value.as_uint);
						break;
					case reshadefx::type::t_float:
						preset.get(effect_name, spec_constant.name, spec_constant.initializer_value.as_float);
						break;
					}

					spec_constant_attributes += spec_constant.name;
					for (uint32_t i = 0; i < spec_constant.size / 4; ++i)
						spec_constant_attributes += std::to_string(spec_constant.initializer_value.as_uint[i]);
				}

				spec_constants_hash = std::hash<std::string>()(spec_constant_attributes);

				// Update specialization constant values for when code is generated below in 'finalize_code' and 'assemble_code_for_entry_point'
				codegen->module().spec_constants = permutation.module.spec_constants;
			}
		}
		else if (!preprocessed)
		{
			assert(!preprocess_required);

			return load_effect(source_file, preset, effect_index, permutation_index, force_load, true);
		}

		permutation.generated_code = codegen->finalize_code();
	}

	if ((preprocessed || source_cached) && compiled)
	{
		if (permutation.cso.empty())
		{
			// Compile shader modules
			for (const std::pair<std::string, reshadefx::shader_type> &entry_point : permutation.module.entry_points)
			{
				if (entry_point.second == reshadefx::shader_type::compute && !_device->check_capability(api::device_caps::compute_shader))
				{
					errors += "error: " + entry_point.first + ": compute shaders are not supported in D3D9/D3D10\n";
					compiled = false;
					break;
				}

				std::string &cso = permutation.cso[entry_point.first];
				std::string &assembly = permutation.assembly[entry_point.first];

				const std::string cache_id = source_file.stem().u8string() + '-' + std::to_string(_renderer_id) + '-' + std::to_string(source_hash) + '-' + std::to_string(spec_constants_hash) + '-' + entry_point.first;

				if (load_effect_cache(cache_id, "cso", cso) &&
					load_effect_cache(cache_id, "asm", assembly))
				{
					continue;
				}
				else
				{
					if (!codegen->assemble_code_for_entry_point(entry_point.first, cso, assembly, errors))
					{
						compiled = false;
						break;
					}

					save_effect_cache(cache_id, "cso", cso);
					save_effect_cache(cache_id, "asm", assembly);
				}
			}
		}

		const std::unique_lock<std::shared_mutex> lock(_reload_mutex);

		for (texture new_texture : permutation.module.textures)
		{
			if (!new_texture.semantic.empty() && (new_texture.render_target || new_texture.storage_access))
			{
				errors += "error: " + new_texture.unique_name + ": texture with a semantic used as a render target or storage\n";
				compiled = false;
				break;
			}

			// Try to share textures with the same name across effects
			if (const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
					[&new_texture](const texture &item) {
						return item.unique_name == new_texture.unique_name;
					});
				existing_texture != _textures.end())
			{
				const bool shared_permutation = std::find(existing_texture->shared.begin(), existing_texture->shared.end(), effect_index) != existing_texture->shared.end();

				// Cannot share texture if this is a normal one, but the existing one is a reference and vice versa
				if (new_texture.semantic != existing_texture->semantic)
				{
					errors += "error: " + new_texture.unique_name + ":"
						" another effect " + (shared_permutation ? "permutation" : '(' + _effects[existing_texture->shared[0]].source_file.filename().u8string() + ')') +
						" already created a texture with the same name but different semantic\n";
					compiled = false;
					break;
				}

				if (new_texture.semantic.empty() && !existing_texture->matches_description(new_texture))
				{
					errors += "warning: " + new_texture.unique_name + ":"
						" another effect " + (shared_permutation ? "permutation" : '(' + _effects[existing_texture->shared[0]].source_file.filename().u8string() + ')') +
						" already created a texture with the same name but different dimensions\n";
				}
				if (new_texture.semantic.empty() && (existing_texture->annotation_as_string("source") != new_texture.annotation_as_string("source")))
				{
					errors += "warning: " + new_texture.unique_name + ":"
						" another effect " + (shared_permutation ? "permutation" : '(' + _effects[existing_texture->shared[0]].source_file.filename().u8string() + ')') +
						" already created a texture with a different image file\n";
				}

				if (existing_texture->semantic == "COLOR" && api::format_bit_depth(_effect_permutations[permutation_index].color_format) != 8)
				{
					for (const reshadefx::sampler &sampler_info : permutation.module.samplers)
					{
						if (sampler_info.srgb && sampler_info.texture_name == new_texture.unique_name)
						{
							errors += "warning: " + sampler_info.unique_name + ": texture does not support sRGB sampling (back buffer format is not RGBA8)\n";
						}
					}
				}

				if (!shared_permutation)
					existing_texture->shared.push_back(effect_index);

				// Update render target and storage access flags of the existing shared texture, in case they are used as such in this effect
				existing_texture->render_target |= new_texture.render_target;
				existing_texture->storage_access |= new_texture.storage_access;
				continue;
			}

			if (new_texture.annotation_as_int("pooled") && new_texture.semantic.empty())
			{
				// Try to find another pooled texture to share with (and do not share within the same effect)
				if (const auto existing_texture = std::find_if(_textures.begin(), _textures.end(),
						[effect_index, &new_texture](const texture &item) {
							return item.annotation_as_int("pooled") && std::find(item.shared.begin(), item.shared.end(), effect_index) == item.shared.end() && item.matches_description(new_texture);
						});
					existing_texture != _textures.end())
				{
					// Overwrite referenced texture in samplers with the pooled one
					for (reshadefx::sampler &sampler_info : permutation.module.samplers)
					{
						if (new_texture.unique_name == sampler_info.texture_name)
							sampler_info.texture_name = existing_texture->unique_name;
					}

					// Overwrite referenced texture in storages with the pooled one
					for (reshadefx::storage &storage_info : permutation.module.storages)
					{
						if (new_texture.unique_name == storage_info.texture_name)
							storage_info.texture_name = existing_texture->unique_name;
					}

					// Overwrite referenced texture in render targets with the pooled one
					for (reshadefx::technique &tech : permutation.module.techniques)
					{
						for (reshadefx::pass &pass : tech.passes)
						{
							std::replace(std::begin(pass.render_target_names), std::end(pass.render_target_names), new_texture.unique_name, existing_texture->unique_name);
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

		for (technique new_technique : permutation.module.techniques)
		{
			new_technique.effect_index = effect_index;

			if (const auto existing_technique = std::find_if(_techniques.begin(), _techniques.end(),
					[&new_technique](const technique &item) {
						return item.effect_index == new_technique.effect_index && item.name == new_technique.name;
					});
					existing_technique != _techniques.end())
			{
				existing_technique->permutations.resize(effect.permutations.size());
				if (existing_technique->permutations[permutation_index].created == false)
					existing_technique->permutations[permutation_index] = std::move(new_technique.permutations[0]);

				// Merge annotations (this can cause duplicated entries, but that's fine, 'annotation_as_*' will just always return the first one)
				existing_technique->annotations.insert(existing_technique->annotations.end(), new_technique.annotations.begin(), new_technique.annotations.end());
				continue;
			}

			assert(permutation_index == 0);

			new_technique.hidden = new_technique.annotation_as_int("hidden") != 0;
			new_technique.enabled_in_screenshot = new_technique.annotation_as_int("enabled_in_screenshot", 0, true) != 0;

			if (new_technique.annotation_as_int("enabled"))
				enable_technique(new_technique);

			_techniques.push_back(std::move(new_technique));
			_technique_sorting.push_back(_techniques.size() - 1);
		}
	}

	effect.compiled = compiled;

	if (!errors.empty())
		effect.errors = std::move(errors);

	const std::chrono::high_resolution_clock::time_point time_load_finished = std::chrono::high_resolution_clock::now();

	if (_reload_remaining_effects != std::numeric_limits<size_t>::max())
	{
		assert(_reload_remaining_effects != 0);
		_reload_remaining_effects--;
	}

	if (compiled && (preprocessed || source_cached))
	{
		if (effect.errors.empty())
			log::message(log::level::info, "Successfully compiled '%s'%s in %f s.", source_file.u8string().c_str(), permutation_index == 0 ? "" : " permutation", std::chrono::duration_cast<std::chrono::milliseconds>(time_load_finished - time_load_started).count() * 1e-3f);
		else
			log::message(log::level::warning, "Successfully compiled '%s'%s in %f s with warnings:\n%s", source_file.u8string().c_str(), permutation_index == 0 ? "" : " permutation", std::chrono::duration_cast<std::chrono::milliseconds>(time_load_finished - time_load_started).count() * 1e-3f, effect.errors.c_str());
		return true;
	}
	else
	{
		_last_reload_successful = false;

		if (effect.errors.empty())
			log::message(log::level::error, "Failed to compile '%s'%s!", source_file.u8string().c_str(), permutation_index == 0 ? "" : " permutation");
		else
			log::message(log::level::error, "Failed to compile '%s'%s:\n%s", source_file.u8string().c_str(), permutation_index == 0 ? "" : " permutation", effect.errors.c_str());
		return false;
	}
}
bool reshade::runtime::create_effect(size_t effect_index, size_t permutation_index)
{
	effect &effect = _effects[effect_index];

	if (!effect.compiled)
		return false;

	// Cannot create an effect that was not previously destroyed (ignore other permutations, since the value is already set by the default permutation)
	assert(!effect.created || permutation_index != 0);

	effect::permutation &permutation = effect.permutations[permutation_index];

	// Create textures now, since they are referenced when building samplers below
	for (texture &tex : _textures)
	{
		if (std::find(tex.shared.cbegin(), tex.shared.cend(), effect_index) == tex.shared.cend())
			continue;

		if (tex.resource != 0)
		{
			if (!(tex.render_target && tex.rtv[0] == 0) &&
				!(tex.storage_access && _renderer_id >= 0xb000 && tex.uav.empty()))
				continue;

			// Update texture if usage has changed since it was last created (e.g. because a pooled texture is now used with storage access when it was not before)
			destroy_texture(tex);

			// This also requires the descriptors to be updated in all effects referencing this texture, so simply recreate them
			for (size_t shared_effect_index : tex.shared)
			{
				if (shared_effect_index == effect_index)
					continue;

				if (std::find(_reload_create_queue.cbegin(), _reload_create_queue.cend(), std::make_pair(shared_effect_index, permutation_index)) == _reload_create_queue.cend())
				{
					destroy_effect(shared_effect_index, false);
					_reload_create_queue.emplace_back(shared_effect_index, permutation_index);
				}
			}
		}

		if (!create_texture(tex))
		{
			effect.errors += "error: " + tex.unique_name + ": failed to create texture";
			return false;
		}
	}

	// Build specialization constants
	std::vector<uint32_t> spec_data;
	std::vector<uint32_t> spec_constants;
	for (const reshadefx::uniform &spec_constant : permutation.module.spec_constants)
	{
		uint32_t id = static_cast<uint32_t>(spec_constants.size());
		spec_data.push_back(spec_constant.initializer_value.as_uint[0]);
		spec_constants.push_back(id);
	}

	// Initialize bindings
	const bool sampler_with_resource_view = _device->check_capability(api::device_caps::sampler_with_resource_view);

	api::descriptor_range cb_range;
	cb_range.binding = 0;
	cb_range.dx_register_index = 0; // b0 (global constant buffer)
	cb_range.dx_register_space = 0;
	cb_range.count = 1;
	cb_range.array_size = 1;
	cb_range.type = api::descriptor_type::constant_buffer;
	cb_range.visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	api::descriptor_range sampler_range;
	sampler_range.binding = 0;
	sampler_range.dx_register_index = 0; // s#
	sampler_range.dx_register_space = 0;
	sampler_range.count = 0;
	sampler_range.array_size = 1;
	sampler_range.type = sampler_with_resource_view ? api::descriptor_type::sampler_with_resource_view : api::descriptor_type::sampler;
	sampler_range.visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	api::descriptor_range srv_range;
	srv_range.binding = 0;
	srv_range.dx_register_index = 0; // t#
	srv_range.dx_register_space = 0;
	srv_range.count = 0;
	srv_range.array_size = 1;
	srv_range.type = api::descriptor_type::shader_resource_view;
	srv_range.visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	api::descriptor_range uav_range;
	uav_range.binding = 0;
	uav_range.dx_register_index = 0; // u#
	uav_range.dx_register_space = 0;
	uav_range.count = 0;
	uav_range.array_size = 1;
	uav_range.type = api::descriptor_type::unordered_access_view;
	uav_range.visibility = api::shader_stage::vertex | api::shader_stage::pixel | api::shader_stage::compute;

	size_t total_pass_count = 0;
	for (const reshadefx::technique &tech : permutation.module.techniques)
	{
		total_pass_count += tech.passes.size();

		for (const reshadefx::pass &pass : tech.passes)
		{
			for (const reshadefx::sampler_binding &binding : pass.sampler_bindings)
				sampler_range.count = std::max(sampler_range.count, binding.entry_point_binding + 1);
			for (const reshadefx::texture_binding &binding : pass.texture_bindings)
				srv_range.count = std::max(srv_range.count, binding.entry_point_binding + 1);
			for (const reshadefx::storage_binding &binding : pass.storage_bindings)
				uav_range.count = std::max(uav_range.count, binding.entry_point_binding + 1);
		}
	}

	// Create optional query heap for time measurements
	if (permutation_index == 0 &&
		!_device->create_query_heap(api::query_type::timestamp, static_cast<uint32_t>((permutation.module.techniques.size() + total_pass_count) * 2 * 4), &effect.query_heap))
	{
		log::message(log::level::error, "Failed to create query heap for effect file '%s'!", effect.source_file.u8string().c_str());
	}

	std::vector<api::descriptor_table_update> descriptor_writes;
	descriptor_writes.reserve(
		static_cast<size_t>(cb_range.count) +
		static_cast<size_t>(sampler_range.count) +
		static_cast<size_t>(srv_range.count) +
		static_cast<size_t>(uav_range.count));

	std::vector<api::descriptor_table> shader_resource_view_tables(total_pass_count);
	std::vector<api::descriptor_table> unordered_access_view_tables(total_pass_count);

	uint16_t sampler_list = 0;
	std::vector<api::sampler_with_resource_view> sampler_descriptors;
	sampler_descriptors.resize(std::max(sampler_range.count, srv_range.count) * total_pass_count);

	// Create pipeline layout for this effect
	{
		api::pipeline_layout_param layout_params[4];
		layout_params[0].type = api::pipeline_layout_param_type::descriptor_table;
		layout_params[0].descriptor_table.count = 1;
		layout_params[0].descriptor_table.ranges = &cb_range;

		layout_params[1].type = api::pipeline_layout_param_type::descriptor_table;
		layout_params[1].descriptor_table.count = 1;
		layout_params[1].descriptor_table.ranges = &sampler_range;

		layout_params[2].type = api::pipeline_layout_param_type::descriptor_table;
		layout_params[2].descriptor_table.count = 1;

		layout_params[3].type = api::pipeline_layout_param_type::descriptor_table;
		layout_params[3].descriptor_table.count = 1;

		if (sampler_with_resource_view)
		{
			layout_params[2].descriptor_table.ranges = &uav_range;
		}
		else
		{
			layout_params[2].descriptor_table.ranges = &srv_range;
			layout_params[3].descriptor_table.ranges = &uav_range;
		}

		if (!_device->create_pipeline_layout(sampler_with_resource_view ? 3 : 4, layout_params, &permutation.layout))
		{
			log::message(log::level::error, "Failed to create pipeline layout for effect file '%s'!", effect.source_file.u8string().c_str());
			return false;
		}
	}

	// Create global constant buffer (except in D3D9, which does not have constant buffers)
	api::buffer_range cb_buffer_range = {};
	if (_device->get_api() != api::device_api::d3d9 && !effect.uniform_data_storage.empty())
	{
		if (permutation_index == 0)
		{
			if (!_device->create_resource(
					api::resource_desc(effect.uniform_data_storage.size(), api::memory_heap::cpu_to_gpu, api::resource_usage::constant_buffer),
					nullptr, api::resource_usage::cpu_access, &effect.cb))
			{
				log::message(log::level::error, "Failed to create constant buffer for effect file '%s'!", effect.source_file.u8string().c_str());
				return false;
			}

			_device->set_resource_name(effect.cb, "ReShade constant buffer");
		}
		else
		{
			assert(effect.cb != 0);
		}

		if (!_device->allocate_descriptor_table(permutation.layout, 0, &permutation.cb_table))
		{
			log::message(log::level::error, "Failed to create constant buffer descriptor table for effect file '%s'!", effect.source_file.u8string().c_str());
			return false;
		}

		cb_buffer_range.buffer = effect.cb;

		api::descriptor_table_update &write = descriptor_writes.emplace_back();
		write.table = permutation.cb_table;
		write.binding = 0;
		write.type = api::descriptor_type::constant_buffer;
		write.count = 1;
		write.descriptors = &cb_buffer_range;
	}

	if (sampler_range.count != 0)
	{
		if (!_device->allocate_descriptor_tables(static_cast<uint32_t>(sampler_with_resource_view ? total_pass_count : 1), permutation.layout, 1, sampler_with_resource_view ? shader_resource_view_tables.data() : &permutation.sampler_table))
		{
			log::message(log::level::error, "Failed to create sampler descriptor table for effect file '%s'!", effect.source_file.u8string().c_str());
			return false;
		}
	}
	if (srv_range.count != 0 && !sampler_with_resource_view)
	{
		if (!_device->allocate_descriptor_tables(static_cast<uint32_t>(total_pass_count), permutation.layout, 2, shader_resource_view_tables.data()))
		{
			log::message(log::level::error, "Failed to create texture descriptor table for effect file '%s'!", effect.source_file.u8string().c_str());
			return false;
		}
	}
	if (uav_range.count != 0)
	{
		if (!_device->allocate_descriptor_tables(static_cast<uint32_t>(total_pass_count), permutation.layout, sampler_with_resource_view ? 2 : 3, unordered_access_view_tables.data()))
		{
			log::message(log::level::error, "Failed to create storage descriptor table for effect file '%s'!", effect.source_file.u8string().c_str());
			return false;
		}
	}

	// Initialize techniques and passes
	for (size_t tech_index = 0, pass_index_in_effect = 0, query_base_index = 0; tech_index < _techniques.size(); ++tech_index)
	{
		technique &tech = _techniques[tech_index];

		if (tech.effect_index != effect_index)
			continue;

		assert(permutation_index < tech.permutations.size() && !tech.permutations[permutation_index].created);

		// Offset index so that a query exists for each command frame and two subsequent ones are used for before/after stamps
		if (permutation_index == 0)
		{
			tech.query_base_index = static_cast<uint32_t>(query_base_index);
			query_base_index += (1 + tech.permutations[0].passes.size()) * 2 * 4;
		}

		for (size_t pass_index = 0; pass_index < tech.permutations[permutation_index].passes.size(); ++pass_index, ++pass_index_in_effect)
		{
			technique::pass &pass = tech.permutations[permutation_index].passes[pass_index];
			pass.texture_table = shader_resource_view_tables[pass_index_in_effect];
			pass.storage_table = unordered_access_view_tables[pass_index_in_effect];

			std::vector<api::pipeline_subobject> subobjects;

			if (!pass.cs_entry_point.empty())
			{
				api::shader_desc cs_desc = {};
				const std::string &cs = permutation.cso.at(pass.cs_entry_point);
				cs_desc.code = cs.data();
				cs_desc.code_size = cs.size();
				if (_renderer_id & 0x20000)
				{
					cs_desc.entry_point = pass.cs_entry_point.c_str();
					cs_desc.spec_constants = static_cast<uint32_t>(permutation.module.spec_constants.size());
					cs_desc.spec_constant_ids = spec_constants.data();
					cs_desc.spec_constant_values = spec_data.data();
				}

				subobjects.push_back({ api::pipeline_subobject_type::compute_shader, 1, &cs_desc });

				if (!_device->create_pipeline(permutation.layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &pass.pipeline))
				{
					effect.errors += "error: internal compiler error";

					log::message(log::level::error, "Failed to create compute pipeline for pass %zu in technique '%s' in '%s'!", pass_index, tech.name.c_str(), effect.source_file.u8string().c_str());
					return false;
				}
			}
			else
			{
				api::shader_desc vs_desc = {};
				if (!pass.vs_entry_point.empty())
				{
					const std::string &vs = permutation.cso.at(pass.vs_entry_point);
					vs_desc.code = vs.data();
					vs_desc.code_size = vs.size();
					if (_renderer_id & 0x20000)
					{
						vs_desc.entry_point = pass.vs_entry_point.c_str();
						vs_desc.spec_constants = static_cast<uint32_t>(permutation.module.spec_constants.size());
						vs_desc.spec_constant_ids = spec_constants.data();
						vs_desc.spec_constant_values = spec_data.data();
					}

					subobjects.push_back({ api::pipeline_subobject_type::vertex_shader, 1, &vs_desc });
				}

				api::shader_desc ps_desc = {};
				if (!pass.ps_entry_point.empty())
				{
					const std::string &ps = permutation.cso.at(pass.ps_entry_point);
					ps_desc.code = ps.data();
					ps_desc.code_size = ps.size();
					if (_renderer_id & 0x20000)
					{
						ps_desc.entry_point = pass.ps_entry_point.c_str();
						ps_desc.spec_constants = static_cast<uint32_t>(permutation.module.spec_constants.size());
						ps_desc.spec_constant_ids = spec_constants.data();
						ps_desc.spec_constant_values = spec_data.data();
					}

					subobjects.push_back({ api::pipeline_subobject_type::pixel_shader, 1, &ps_desc });
				}

				api::format render_target_formats[8] = {};
				if (pass.render_target_names[0].empty())
				{
					pass.viewport_width = _effect_permutations[permutation_index].width;
					pass.viewport_height = _effect_permutations[permutation_index].height;

					render_target_formats[0] = api::format_to_default_typed(_effect_permutations[permutation_index].color_format, pass.srgb_write_enable);

					subobjects.push_back({ api::pipeline_subobject_type::render_target_formats, 1, &render_target_formats[0] });
				}
				else
				{
					int render_target_count = 0;
					for (; render_target_count < 8 && !pass.render_target_names[render_target_count].empty(); ++render_target_count)
					{
						const auto render_target_texture = std::find_if(_textures.cbegin(), _textures.cend(),
							[&unique_name = pass.render_target_names[render_target_count]](const texture &item) {
								return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty());
							});
						assert(render_target_texture != _textures.cend());

						const api::resource_view rtv = render_target_texture->rtv[pass.srgb_write_enable];
						assert(rtv != 0 && render_target_texture->semantic.empty());

						pass.render_target_views[render_target_count] = rtv;

						const api::resource_desc res_desc = _device->get_resource_desc(render_target_texture->resource);
						render_target_formats[render_target_count] = api::format_to_default_typed(res_desc.texture.format, pass.srgb_write_enable);

						if (std::find(pass.modified_resources.cbegin(), pass.modified_resources.cend(), render_target_texture->resource) == pass.modified_resources.cend())
						{
							pass.modified_resources.push_back(render_target_texture->resource);

							if (pass.generate_mipmaps && render_target_texture->levels > 1)
								pass.generate_mipmap_views.push_back(render_target_texture->srv[0]);
						}
					}

					subobjects.push_back({ api::pipeline_subobject_type::render_target_formats, static_cast<uint32_t>(render_target_count), render_target_formats });
				}

				// Only need to attach stencil if stencil is actually used in this pass
				if (pass.stencil_enable &&
					pass.viewport_width == _effect_permutations[permutation_index].width &&
					pass.viewport_height == _effect_permutations[permutation_index].height)
				{
					subobjects.push_back({ api::pipeline_subobject_type::depth_stencil_format, 1, &_effect_permutations[permutation_index].stencil_format });
				}

				subobjects.push_back({ api::pipeline_subobject_type::max_vertex_count, 1, &pass.num_vertices });

				api::primitive_topology topology = static_cast<api::primitive_topology>(pass.topology);
				subobjects.push_back({ api::pipeline_subobject_type::primitive_topology, 1, &topology });

				const auto convert_blend_op = [](reshadefx::blend_op value) {
					switch (value)
					{
					default:
					case reshadefx::blend_op::add: return api::blend_op::add;
					case reshadefx::blend_op::subtract: return api::blend_op::subtract;
					case reshadefx::blend_op::reverse_subtract: return api::blend_op::reverse_subtract;
					case reshadefx::blend_op::min: return api::blend_op::min;
					case reshadefx::blend_op::max: return api::blend_op::max;
					}
				};
				const auto convert_blend_factor = [](reshadefx::blend_factor value) {
					switch (value) {
					case reshadefx::blend_factor::zero: return api::blend_factor::zero;
					default:
					case reshadefx::blend_factor::one: return api::blend_factor::one;
					case reshadefx::blend_factor::source_color: return api::blend_factor::source_color;
					case reshadefx::blend_factor::one_minus_source_color: return api::blend_factor::one_minus_source_color;
					case reshadefx::blend_factor::dest_color: return api::blend_factor::dest_color;
					case reshadefx::blend_factor::one_minus_dest_color: return api::blend_factor::one_minus_dest_color;
					case reshadefx::blend_factor::source_alpha: return api::blend_factor::source_alpha;
					case reshadefx::blend_factor::one_minus_source_alpha: return api::blend_factor::one_minus_source_alpha;
					case reshadefx::blend_factor::dest_alpha: return api::blend_factor::dest_alpha;
					case reshadefx::blend_factor::one_minus_dest_alpha: return api::blend_factor::one_minus_dest_alpha;
					}
				};

				// Technically should check for 'api::device_caps::independent_blend' support, but render target write masks are supported in D3D9, when rest is not, so just always set ...
				api::blend_desc blend_state = {};
				for (int i = 0; i < 8; ++i)
				{
					blend_state.blend_enable[i] = pass.blend_enable[i];
					blend_state.source_color_blend_factor[i] = convert_blend_factor(pass.source_color_blend_factor[i]);
					blend_state.dest_color_blend_factor[i] = convert_blend_factor(pass.dest_color_blend_factor[i]);
					blend_state.color_blend_op[i] = convert_blend_op(pass.color_blend_op[i]);
					blend_state.source_alpha_blend_factor[i] = convert_blend_factor(pass.source_alpha_blend_factor[i]);
					blend_state.dest_alpha_blend_factor[i] = convert_blend_factor(pass.dest_alpha_blend_factor[i]);
					blend_state.alpha_blend_op[i] = convert_blend_op(pass.alpha_blend_op[i]);
					blend_state.render_target_write_mask[i] = pass.render_target_write_mask[i];
				}

				subobjects.push_back({ api::pipeline_subobject_type::blend_state, 1, &blend_state });

				api::rasterizer_desc rasterizer_state = {};
				rasterizer_state.cull_mode = api::cull_mode::none;

				subobjects.push_back({ api::pipeline_subobject_type::rasterizer_state, 1, &rasterizer_state });

				const auto convert_stencil_op = [](reshadefx::stencil_op value) {
					switch (value) {
					case reshadefx::stencil_op::zero: return api::stencil_op::zero;
					default:
					case reshadefx::stencil_op::keep: return api::stencil_op::keep;
					case reshadefx::stencil_op::replace: return api::stencil_op::replace;
					case reshadefx::stencil_op::increment_saturate: return api::stencil_op::increment_saturate;
					case reshadefx::stencil_op::decrement_saturate: return api::stencil_op::decrement_saturate;
					case reshadefx::stencil_op::invert: return api::stencil_op::invert;
					case reshadefx::stencil_op::increment: return api::stencil_op::increment;
					case reshadefx::stencil_op::decrement: return api::stencil_op::decrement;
					}
				};
				const auto convert_stencil_func = [](reshadefx::stencil_func value) {
					switch (value)
					{
					case reshadefx::stencil_func::never: return api::compare_op::never;
					case reshadefx::stencil_func::less: return api::compare_op::less;
					case reshadefx::stencil_func::equal: return api::compare_op::equal;
					case reshadefx::stencil_func::less_equal: return api::compare_op::less_equal;
					case reshadefx::stencil_func::greater: return api::compare_op::greater;
					case reshadefx::stencil_func::not_equal: return api::compare_op::not_equal;
					case reshadefx::stencil_func::greater_equal: return api::compare_op::greater_equal;
					default:
					case reshadefx::stencil_func::always: return api::compare_op::always;
					}
				};

				api::depth_stencil_desc depth_stencil_state = {};
				depth_stencil_state.depth_enable = false;
				depth_stencil_state.depth_write_mask = false;
				depth_stencil_state.depth_func = api::compare_op::always;
				depth_stencil_state.stencil_enable = pass.stencil_enable;
				depth_stencil_state.front_stencil_read_mask = pass.stencil_read_mask;
				depth_stencil_state.front_stencil_write_mask = pass.stencil_write_mask;
				depth_stencil_state.front_stencil_func = convert_stencil_func(pass.stencil_comparison_func);
				depth_stencil_state.front_stencil_fail_op = convert_stencil_op(pass.stencil_fail_op);
				depth_stencil_state.front_stencil_depth_fail_op = convert_stencil_op(pass.stencil_depth_fail_op);
				depth_stencil_state.front_stencil_pass_op = convert_stencil_op(pass.stencil_pass_op);
				depth_stencil_state.back_stencil_read_mask = depth_stencil_state.front_stencil_read_mask;
				depth_stencil_state.back_stencil_write_mask = depth_stencil_state.front_stencil_write_mask;
				depth_stencil_state.back_stencil_func = depth_stencil_state.front_stencil_func;
				depth_stencil_state.back_stencil_fail_op = depth_stencil_state.front_stencil_fail_op;
				depth_stencil_state.back_stencil_depth_fail_op = depth_stencil_state.front_stencil_depth_fail_op;
				depth_stencil_state.back_stencil_pass_op = depth_stencil_state.front_stencil_pass_op;

				subobjects.push_back({ api::pipeline_subobject_type::depth_stencil_state, 1, &depth_stencil_state });

				if (!_device->create_pipeline(permutation.layout, static_cast<uint32_t>(subobjects.size()), subobjects.data(), &pass.pipeline))
				{
					effect.errors += "error: internal compiler error";

					log::message(log::level::error, "Failed to create graphics pipeline for pass %zu in technique '%s' in '%s'!", pass_index, tech.name.c_str(), effect.source_file.u8string().c_str());
					return false;
				}
			}

			for (const reshadefx::sampler_binding &binding : pass.sampler_bindings)
			{
				if (!sampler_with_resource_view)
				{
					// Maximum sampler slot count is 16, so a 16-bit integer is enough to hold all bindings
					assert(binding.entry_point_binding < 16);

					// Only initialize sampler if it has not been created before
					if ((sampler_list & (1 << binding.entry_point_binding)) != 0)
						continue;

					sampler_list |= (1 << binding.entry_point_binding);
				}

				api::sampler &sampler = sampler_descriptors[pass_index_in_effect * sampler_range.count + binding.entry_point_binding].sampler;

				const reshadefx::sampler &sampler_info = permutation.module.samplers[binding.index];

				api::sampler_desc desc;
				desc.filter = static_cast<api::filter_mode>(sampler_info.filter);
				desc.address_u = static_cast<api::texture_address_mode>(sampler_info.address_u);
				desc.address_v = static_cast<api::texture_address_mode>(sampler_info.address_v);
				desc.address_w = static_cast<api::texture_address_mode>(sampler_info.address_w);
				desc.mip_lod_bias = sampler_info.lod_bias;
				desc.max_anisotropy = (desc.filter == api::filter_mode::anisotropic || desc.filter == api::filter_mode::min_mag_anisotropic_mip_point) ? 16.0f : 1.0f;
				desc.compare_op = api::compare_op::never;
				desc.border_color[0] = 0.0f;
				desc.border_color[1] = 0.0f;
				desc.border_color[2] = 0.0f;
				desc.border_color[3] = 0.0f;
				desc.min_lod = sampler_info.min_lod;
				desc.max_lod = sampler_info.max_lod;

				// Generate hash for sampler description
				size_t desc_hash = 2166136261;
				for (int i = 0; i < sizeof(desc); ++i)
					desc_hash = (desc_hash * 16777619) ^ reinterpret_cast<const uint8_t *>(&desc)[i];

				if (const auto it = _effect_sampler_states.find(desc_hash);
					it != _effect_sampler_states.end())
				{
					sampler = it->second;
				}
				else if (_device->create_sampler(desc, &sampler))
				{
					_effect_sampler_states.emplace(desc_hash, sampler);
				}
				else
				{
					log::message(log::level::error, "Failed to create sampler object in '%s'!", effect.source_file.u8string().c_str());
					return false;
				}

				api::descriptor_table_update &write = descriptor_writes.emplace_back();
				write.table = sampler_with_resource_view ? pass.texture_table : permutation.sampler_table;
				write.binding = binding.entry_point_binding;
				write.count = 1;
				write.type = sampler_with_resource_view ? api::descriptor_type::sampler_with_resource_view : api::descriptor_type::sampler;
				write.descriptors = &sampler;
			}

			for (const reshadefx::texture_binding &binding : pass.texture_bindings)
			{
				const auto sampler_texture = std::find_if(_textures.cbegin(), _textures.cend(),
					[&unique_name = permutation.module.samplers[binding.index].texture_name](const texture &item) {
						return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty());
					});
				assert(sampler_texture != _textures.cend());

				api::resource_view &srv = sampler_descriptors[pass_index_in_effect * srv_range.count + binding.entry_point_binding].view;

				if (sampler_with_resource_view)
				{
					// The sampler and descriptor table update for this 'sampler_with_resource_view' descriptor were already initialized above
					assert(
						srv_range.count == sampler_range.count &&
						sampler_descriptors[pass_index_in_effect * srv_range.count + binding.entry_point_binding].sampler != 0);
				}
				else
				{
					api::descriptor_table_update &write = descriptor_writes.emplace_back();
					write.table = pass.texture_table;
					write.binding = binding.entry_point_binding;
					write.count = 1;
					write.type = api::descriptor_type::shader_resource_view;
					write.descriptors = &srv;
				}

				if (!sampler_texture->semantic.empty())
				{
					if (sampler_texture->semantic == "COLOR")
						srv = _effect_permutations[permutation_index].color_srv[binding.srgb];
					else if (const auto it = _texture_semantic_bindings.find(sampler_texture->semantic); it != _texture_semantic_bindings.end())
						srv = binding.srgb ? it->second.second : it->second.first;
					else
						srv = _empty_srv;

					// Keep track of the texture descriptor to simplify updating it
					permutation.texture_semantic_to_binding.push_back({
						sampler_texture->semantic,
						pass.texture_table,
						binding.entry_point_binding,
						sampler_with_resource_view ? sampler_descriptors[pass_index_in_effect * srv_range.count + binding.entry_point_binding].sampler : api::sampler { 0 },
						binding.srgb
					});
				}
				else
				{
					srv = sampler_texture->srv[binding.srgb];
				}

				assert(srv != 0);
			}

			for (const reshadefx::storage_binding &binding : pass.storage_bindings)
			{
				const auto storage_texture = std::find_if(_textures.cbegin(), _textures.cend(),
					[&unique_name = permutation.module.storages[binding.index].texture_name](const texture &item) {
						return item.unique_name == unique_name && (item.resource != 0 || !item.semantic.empty());
					});
				assert(storage_texture != _textures.cend());

				const api::resource_view &uav = storage_texture->uav[permutation.module.storages[binding.index].level];
				assert(uav != 0 && storage_texture->semantic.empty());

				{
					api::descriptor_table_update &write = descriptor_writes.emplace_back();
					write.table = pass.storage_table;
					write.binding = binding.entry_point_binding;
					write.count = 1;
					write.type = api::descriptor_type::unordered_access_view;
					write.descriptors = &uav;
				}

				if (std::find(pass.modified_resources.cbegin(), pass.modified_resources.cend(), storage_texture->resource) == pass.modified_resources.cend())
				{
					pass.modified_resources.push_back(storage_texture->resource);

					if (pass.generate_mipmaps && storage_texture->levels > 1)
						pass.generate_mipmap_views.push_back(storage_texture->srv[0]);
				}
			}
		}

		tech.permutations[permutation_index].created = true;
	}

	if (!descriptor_writes.empty())
		_device->update_descriptor_tables(static_cast<uint32_t>(descriptor_writes.size()), descriptor_writes.data());

	effect.created = true;

	load_textures(effect_index);

	return true;
}
void reshade::runtime::destroy_effect(size_t effect_index, bool unload)
{
	assert(effect_index < _effects.size());

	for (technique &tech : _techniques)
	{
		if (tech.effect_index != effect_index)
			continue;

		for (technique::permutation &permutation : tech.permutations)
		{
			for (technique::pass &pass : permutation.passes)
			{
				_device->destroy_pipeline(pass.pipeline);
				pass.pipeline = {};

				_device->free_descriptor_table(pass.texture_table);
				pass.texture_table = {};
				_device->free_descriptor_table(pass.storage_table);
				pass.storage_table = {};

				std::fill_n(pass.render_target_views, 8, api::resource_view {});
				pass.modified_resources.clear();
				pass.generate_mipmap_views.clear();
			}

			permutation.created = false;
		}
	}

	effect &effect = _effects[effect_index];
	{
		_device->destroy_resource(effect.cb);
		effect.cb = {};

		_device->destroy_query_heap(effect.query_heap);
		effect.query_heap = {};

		for (effect::permutation &permutation : effect.permutations)
		{
			_device->free_descriptor_table(permutation.cb_table);
			permutation.cb_table = {};
			_device->free_descriptor_table(permutation.sampler_table);
			permutation.sampler_table = {};

			_device->destroy_pipeline_layout(permutation.layout);
			permutation.layout = {};

			permutation.texture_semantic_to_binding.clear();
		}

		effect.created = false;
	}

	if (!unload)
		return;

	// Lock here to be safe in case another effect is still loading
	const std::unique_lock<std::shared_mutex> lock(_reload_mutex);

	// No techniques from this effect are rendering anymore
	effect.rendering = 0;

	// Destroy textures belonging to this effect
	_textures.erase(std::remove_if(_textures.begin(), _textures.end(),
		[this, effect_index](texture &tex) {
			tex.shared.erase(std::remove(tex.shared.begin(), tex.shared.end(), effect_index), tex.shared.end());
			if (tex.shared.empty())
			{
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

	// Do not clear effect here, since it is common to be reused immediately
}

void reshade::runtime::load_textures(size_t effect_index)
{
	for (texture &tex : _textures)
	{
		if (tex.resource == 0 || !tex.semantic.empty())
			continue; // Ignore textures that are not created yet and those that are handled in the runtime implementation
		if (std::find(tex.shared.begin(), tex.shared.end(), effect_index) == tex.shared.end())
			continue; // Ignore textures not being used with this effect

		std::filesystem::path source_path = std::filesystem::u8path(tex.annotation_as_string("source"));
		// Ignore textures that have no image file attached to them (e.g. plain render targets)
		if (source_path.empty())
			continue;

		// Search for image file using the provided search paths unless the path provided is already absolute
		if (!find_file(_texture_search_paths, source_path))
		{
			log::message(log::level::error, "Source '%s' for texture '%s' was not found in any of the texture search paths!", source_path.u8string().c_str(), tex.unique_name.c_str());
			_last_reload_successful = false;
			continue;
		}

		void *pixels = nullptr;
		int width = 0, height = 1, depth = 1, channels = 0;
		const bool is_floating_point_format =
			tex.format == reshadefx::texture_format::r32f ||
			tex.format == reshadefx::texture_format::rg32f ||
			tex.format == reshadefx::texture_format::rgba32f;

		if (FILE *const file = _wfsopen(source_path.c_str(), L"rb", SH_DENYNO))
		{
			fseek(file, 0, SEEK_END);
			const size_t file_size = ftell(file);
			fseek(file, 0, SEEK_SET);

			if (source_path.extension() == L".cube")
			{
				if (!is_floating_point_format)
				{
					log::message(log::level::error, "Source '%s' for texture '%s' is a Cube LUT file, which can only be loaded into textures with a floating-point format!", source_path.u8string().c_str(), tex.unique_name.c_str());
					_last_reload_successful = false;
					continue;
				}

				float domain_min[3] = { 0.0f, 0.0f, 0.0f };
				float domain_max[3] = { 1.0f, 1.0f, 1.0f };

				// Read header information
				char line_data[1024];
				while (fgets(line_data, sizeof(line_data), file))
				{
					const std::string_view line = trim(line_data, "\r\n");

					if (line.empty() || line[0] == '#')
						continue; // Skip lines with comments

					char *p = line_data;

					if (line.rfind("TITLE", 0) == 0)
						continue; // Skip optional line with title

					if (line.rfind("DOMAIN_MIN", 0) == 0)
					{
						p += 10;
						domain_min[0] = static_cast<float>(std::strtod(p, &p));
						domain_min[1] = static_cast<float>(std::strtod(p, &p));
						domain_min[2] = static_cast<float>(std::strtod(p, &p));
						continue;
					}
					if (line.rfind("DOMAIN_MAX", 0) == 0)
					{
						p += 10;
						domain_max[0] = static_cast<float>(std::strtod(p, &p));
						domain_max[1] = static_cast<float>(std::strtod(p, &p));
						domain_max[2] = static_cast<float>(std::strtod(p, &p));
						continue;
					}

					if (line.rfind("LUT_1D_SIZE", 0) == 0)
					{
						if (pixels != nullptr)
							break;
						width = std::strtol(p + 11, nullptr, 10);
						pixels = std::malloc(static_cast<size_t>(width) * 4 * sizeof(float));
						continue;
					}
					if (line.rfind("LUT_3D_SIZE", 0) == 0)
					{
						if (pixels != nullptr)
							break;
						width = height = depth = std::strtol(p + 11, nullptr, 10);
						pixels = std::malloc(static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4 * sizeof(float));
						continue;
					}

					// Line has no known keyword, so assume this is where the table data starts and roll back a line to continue reading that below
					fseek(file, -static_cast<long>(std::strlen(line_data)), SEEK_CUR);
					break;
				}

				// Read table data
				if (pixels != nullptr)
				{
					size_t index = 0;

					while (fgets(line_data, sizeof(line_data), file) && (index + 4) <= (static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4))
					{
						const std::string_view line = trim(line_data, "\r\n");

						if (line.empty() || line[0] == '#')
							continue; // Skip lines with comments

						char *p = line_data;

						static_cast<float *>(pixels)[index++] = static_cast<float>(std::strtod(p, &p)) * (domain_max[0] - domain_min[0]) + domain_min[0];
						static_cast<float *>(pixels)[index++] = static_cast<float>(std::strtod(p, &p)) * (domain_max[1] - domain_min[1]) + domain_min[1];
						static_cast<float *>(pixels)[index++] = static_cast<float>(std::strtod(p, &p)) * (domain_max[2] - domain_min[2]) + domain_min[2];
						static_cast<float *>(pixels)[index++] = 1.0f;
					}
				}
			}
			else
			{
				// Read texture data into memory in one go since that is faster than reading chunk by chunk
				std::vector<stbi_uc> file_data(file_size);
				const size_t file_size_read = fread(file_data.data(), 1, file_size, file);
				fclose(file);

				if (file_size_read == file_size)
				{
					if (is_floating_point_format)
						pixels = stbi_loadf_from_memory(file_data.data(), static_cast<int>(file_data.size()), &width, &height, &channels, STBI_rgb_alpha);
					else if (stbi_dds_test_memory(file_data.data(), static_cast<int>(file_data.size())))
						pixels = stbi_dds_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &width, &height, &depth, &channels, STBI_rgb_alpha);
					else
						pixels = stbi_load_from_memory(file_data.data(), static_cast<int>(file_data.size()), &width, &height, &channels, STBI_rgb_alpha);
				}
			}
		}

		if (pixels == nullptr)
		{
			log::message(log::level::error, "Failed to load '%s' for texture '%s'!", source_path.u8string().c_str(), tex.unique_name.c_str());
			_last_reload_successful = false;
			continue;
		}

		// Collapse data to the correct number of components per pixel based on the texture format
		switch (tex.format)
		{
		case reshadefx::texture_format::r8:
			for (size_t i = 4, k = 1; i < static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4; i += 4, k += 1)
				static_cast<stbi_uc *>(pixels)[k] = static_cast<stbi_uc *>(pixels)[i];
			break;
		case reshadefx::texture_format::r32f:
			for (size_t i = 4, k = 1; i < static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4; i += 4, k += 1)
				static_cast<float *>(pixels)[k] = static_cast<float *>(pixels)[i];
			break;
		case reshadefx::texture_format::rg8:
			for (size_t i = 4, k = 2; i < static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4; i += 4, k += 2)
				static_cast<stbi_uc *>(pixels)[k + 0] = static_cast<stbi_uc *>(pixels)[i + 0],
				static_cast<stbi_uc *>(pixels)[k + 1] = static_cast<stbi_uc *>(pixels)[i + 1];
			break;
		case reshadefx::texture_format::rg32f:
			for (size_t i = 4, k = 2; i < static_cast<size_t>(width) * static_cast<size_t>(height) * static_cast<size_t>(depth) * 4; i += 4, k += 2)
				static_cast<float *>(pixels)[k + 0] = static_cast<float *>(pixels)[i + 0],
				static_cast<float *>(pixels)[k + 1] = static_cast<float *>(pixels)[i + 1];
			break;
		case reshadefx::texture_format::rgba8:
		case reshadefx::texture_format::rgba32f:
			break;
		default:
			log::message(log::level::error, "Texture upload is not supported for format %d of texture '%s'!", static_cast<int>(tex.format), tex.unique_name.c_str());
			_last_reload_successful = false;
			stbi_image_free(pixels);
			continue;
		}

		update_texture(tex, width, height, depth, pixels);

		stbi_image_free(pixels);

		tex.loaded = true;
	}
}
bool reshade::runtime::create_texture(texture &tex)
{
	// Do not create resource if it is a special reference, those are set in 'render_technique' and 'update_texture_bindings'
	if (!tex.semantic.empty())
		return true;

	api::resource_type type = api::resource_type::unknown;
	api::resource_view_type view_type = api::resource_view_type::unknown;

	switch (tex.type)
	{
	case reshadefx::texture_type::texture_1d:
		type = api::resource_type::texture_1d;
		view_type = api::resource_view_type::texture_1d;
		break;
	case reshadefx::texture_type::texture_2d:
		type = api::resource_type::texture_2d;
		view_type = api::resource_view_type::texture_2d;
		break;
	case reshadefx::texture_type::texture_3d:
		type = api::resource_type::texture_3d;
		view_type = api::resource_view_type::texture_3d;
		break;
	}

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
	case reshadefx::texture_format::r32i:
		format = api::format::r32_sint;
		break;
	case reshadefx::texture_format::r32u:
		format = api::format::r32_uint;
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
	case reshadefx::texture_format::rgba32i:
		format = api::format::r32g32b32a32_sint;
		break;
	case reshadefx::texture_format::rgba32u:
		format = api::format::r32g32b32a32_uint;
		break;
	case reshadefx::texture_format::rgba32f:
		format = api::format::r32g32b32a32_float;
		break;
	case reshadefx::texture_format::rgb10a2:
		format = api::format::r10g10b10a2_unorm;
		break;
	case reshadefx::texture_format::rg11b10f:
		format = api::format::r11g11b10_float;
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
	const float clear_color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

	std::vector<uint8_t> zero_data;
	std::vector<api::subresource_data> initial_data;
	if (!tex.render_target)
	{
		zero_data.resize(static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height) * static_cast<size_t>(tex.depth) * 16);
		initial_data.resize(tex.levels);
		for (uint32_t level = 0, width = tex.width, height = tex.height; level < tex.levels; ++level, width /= 2, height /= 2)
		{
			initial_data[level].data = zero_data.data();
			initial_data[level].row_pitch = width * 16;
			initial_data[level].slice_pitch = initial_data[level].row_pitch * height;
		}
	}

	if (!_device->create_resource(api::resource_desc(type, tex.width, tex.height, tex.depth, tex.levels, format, 1, api::memory_heap::gpu_only, usage, flags), initial_data.data(), api::resource_usage::shader_resource, &tex.resource))
	{
		log::message(log::level::error, "Failed to create texture '%s' (width = %u, height = %u, levels = %hu, format = %u, usage = %#x)! Make sure the texture dimensions are reasonable.", tex.unique_name.c_str(), tex.width, tex.height, tex.levels, static_cast<uint32_t>(format), static_cast<uint32_t>(usage));
		return false;
	}

	_device->set_resource_name(tex.resource, tex.unique_name.c_str());

	// Always create shader resource views
	{
		if (!_device->create_resource_view(tex.resource, api::resource_usage::shader_resource, api::resource_view_desc(view_type, view_format, 0, tex.levels, 0, UINT32_MAX), &tex.srv[0]))
		{
			log::message(log::level::error, "Failed to create shader resource view for texture '%s' (format = %u, levels = %hu)!", tex.unique_name.c_str(), static_cast<uint32_t>(view_format), tex.levels);
			return false;
		}
		if (view_format_srgb == view_format || tex.storage_access) // sRGB formats do not support storage usage
		{
			tex.srv[1] = tex.srv[0];
		}
		else if (!_device->create_resource_view(tex.resource, api::resource_usage::shader_resource, api::resource_view_desc(view_type, view_format_srgb, 0, tex.levels, 0, UINT32_MAX), &tex.srv[1]))
		{
			log::message(log::level::error, "Failed to create shader resource view for texture '%s' (format = %u, levels = %hu)!", tex.unique_name.c_str(), static_cast<uint32_t>(view_format_srgb), tex.levels);
			return false;
		}
	}

	// Create render target views (with a single level)
	if (tex.render_target)
	{
		if (!_device->create_resource_view(tex.resource, api::resource_usage::render_target, api::resource_view_desc(view_format), &tex.rtv[0]))
		{
			log::message(log::level::error, "Failed to create render target view for texture '%s' (format = %u)!", tex.unique_name.c_str(), static_cast<uint32_t>(view_format));
			return false;
		}
		if (view_format_srgb == view_format || tex.storage_access) // sRGB formats do not support storage usage
		{
			tex.rtv[1] = tex.rtv[0];
		}
		else if (!_device->create_resource_view(tex.resource, api::resource_usage::render_target, api::resource_view_desc(view_format_srgb), &tex.rtv[1]))
		{
			log::message(log::level::error, "Failed to create render target view for texture '%s' (format = %u)!", tex.unique_name.c_str(), static_cast<uint32_t>(view_format_srgb));
			return false;
		}

		api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
		cmd_list->barrier(tex.resource, api::resource_usage::shader_resource, api::resource_usage::render_target);
		cmd_list->clear_render_target_view(tex.rtv[0], clear_color);
		cmd_list->barrier(tex.resource, api::resource_usage::render_target, api::resource_usage::shader_resource);

		if (tex.levels > 1)
			cmd_list->generate_mipmaps(tex.srv[0]);
	}

	if (tex.storage_access && _renderer_id >= 0xb000)
	{
		tex.uav.resize(tex.levels);

		for (uint16_t level = 0; level < tex.levels; ++level)
		{
			if (!_device->create_resource_view(tex.resource, api::resource_usage::unordered_access, api::resource_view_desc(view_type, view_format, level, 1, 0, UINT32_MAX), &tex.uav[level]))
			{
				log::message(log::level::error, "Failed to create unordered access view for texture '%s' (format = %u, level = %hu)!", tex.unique_name.c_str(), static_cast<uint32_t>(view_format), level);
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
	if (!is_loading() && invoke_addon_event<addon_event::reshade_set_technique_state>(this, api::effect_technique { reinterpret_cast<uintptr_t>(&tech) }, true))
		return;
#endif

	const bool status_changed = !tech.enabled;
	tech.enabled = true;
	tech.time_left = tech.annotation_as_int("timeout");

	// Queue effect file for initialization if it was not fully loaded yet
	if (!tech.permutations[0].created &&
		// Avoid adding the same effect multiple times to the queue if it contains multiple techniques that were enabled simultaneously
		std::find(_reload_create_queue.cbegin(), _reload_create_queue.cend(), std::make_pair(tech.effect_index, static_cast<size_t>(0u))) == _reload_create_queue.cend())
		_reload_create_queue.emplace_back(tech.effect_index, static_cast<size_t>(0u));

	if (status_changed) // Increase rendering reference count
		_effects[tech.effect_index].rendering++;
}
void reshade::runtime::disable_technique(technique &tech)
{
	assert(tech.effect_index < _effects.size());

#if RESHADE_ADDON
	if (!is_loading() && invoke_addon_event<addon_event::reshade_set_technique_state>(this, api::effect_technique { reinterpret_cast<uintptr_t>(&tech) }, false))
		return;
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
	if (!is_loading())
	{
		std::vector<api::effect_technique> techniques(technique_indices.size());
		std::transform(technique_indices.cbegin(), technique_indices.cend(), techniques.begin(),
			[this](size_t technique_index) {
				return api::effect_technique { reinterpret_cast<uint64_t>(&_techniques[technique_index]) };
			});

		if (invoke_addon_event<addon_event::reshade_reorder_techniques>(this, techniques.size(), techniques.data()))
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

void reshade::runtime::load_effects(bool force_load_all)
{
	// Build a list of effect files by walking through the effect search paths
	const std::vector<std::filesystem::path> effect_files =
		find_files(_effect_search_paths, { L".fx", L".addonfx" });

	if (effect_files.empty())
		return; // No effect files found, so nothing more to do

	ini_file &preset = ini_file::load_cache(_current_preset_path);

	// Have to be initialized at this point or else the threads spawned below will immediately exit without reducing the remaining effects count
	assert(_is_initialized);

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
	size_t num_splits = std::min(effect_files.size(), static_cast<size_t>(std::max(std::thread::hardware_concurrency(), 2u) - 1));
#ifndef _WIN64
	// Limit number of threads in 32-bit due to the limited about of address space being available there and compilation being memory hungry
	num_splits = std::min(num_splits, static_cast<size_t>(4));
#endif

	// Keep track of the spawned threads, so the runtime cannot be destroyed while they are still running
	for (size_t n = 0; n < num_splits; ++n)
		_worker_threads.emplace_back([this, effect_files, offset, num_splits, n, &preset, force_load_all]() {
			// Abort loading when initialization state changes (indicating that 'on_reset' was called in the meantime)
			for (size_t i = 0; i < effect_files.size() && _is_initialized; ++i)
				if (i * num_splits / effect_files.size() == n)
					load_effect(effect_files[i], preset, offset + i, 0, force_load_all || effect_files[i].extension() == L".addonfx");
		});
}
bool reshade::runtime::reload_effect(size_t effect_index)
{
	assert(!is_loading() || _reload_remaining_effects == 0);

#if RESHADE_GUI
	_show_splash = false; // Hide splash bar when reloading a single effect file
#endif

	// Make sure no effect resources are currently in use
	_graphics_queue->wait_idle();

	const std::filesystem::path source_file = _effects[effect_index].source_file;
	destroy_effect(effect_index);

#if RESHADE_ADDON
	// Call event after destroying the effect, so add-ons get a chance to release any handles they hold to variables and techniques
	invoke_addon_event<addon_event::reshade_reloaded_effects>(this);
#endif

	// Make sure 'is_loading' is true while loading the effect
	_reload_remaining_effects = 1;

	return load_effect(source_file, ini_file::load_cache(_current_preset_path), effect_index, 0, true, true);
}
void reshade::runtime::reload_effects(bool force_load_all)
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
	_last_reload_successful = true;

	load_effects(force_load_all);
}
void reshade::runtime::destroy_effects()
{
	// Make sure no threads are still accessing effect data
	for (std::thread &thread : _worker_threads)
		if (thread.joinable())
			thread.join();
	_worker_threads.clear();

#if RESHADE_GUI
	_effect_filter[0] = '\0';
	_preview_texture = std::numeric_limits<size_t>::max();
#endif

	// Reset the effect creation queue
	_reload_create_queue.clear();
	_reload_required_effects.clear();
	_reload_remaining_effects = std::numeric_limits<size_t>::max();

	// Make sure no effect resources are currently in use (do this even when the effect list is empty, since it is dependent upon by 'on_reset')
	_graphics_queue->wait_idle();

	for (size_t effect_index = 0; effect_index < _effects.size(); ++effect_index)
		destroy_effect(effect_index);

	// Reset the effect list after all resources have been destroyed
	_effects.clear();

	// Clean up sampler objects
	for (const auto &[hash, sampler] : _effect_sampler_states)
		_device->destroy_sampler(sampler);
	_effect_sampler_states.clear();

	// Textures and techniques should have been cleaned up by the calls to 'destroy_effect' above
	assert(_textures.empty());
	assert(_techniques.empty() && _technique_sorting.empty());
}

bool reshade::runtime::load_effect_cache(const std::string &id, const std::string &type, std::string &data) const
{
	if (_no_effect_cache)
		return false;

	std::filesystem::path path = g_reshade_base_path / _effect_cache_path;
	path /= std::filesystem::u8path("reshade-" + id + '.' + type);

	FILE *const file = _wfsopen(path.c_str(), L"rb", SH_DENYNO);
	if (file == nullptr)
		return false;

	fseek(file, 0, SEEK_END);
	const size_t file_size = ftell(file);
	fseek(file, 0, SEEK_SET);

	data.resize(file_size, '\0');
	const size_t file_size_read = fread(data.data(), 1, data.size(), file);
	fclose(file);
	return file_size_read == data.size();
}
bool reshade::runtime::save_effect_cache(const std::string &id, const std::string &type, const std::string &data) const
{
	if (_no_effect_cache)
		return false;

	std::filesystem::path path = g_reshade_base_path / _effect_cache_path;
	path /= std::filesystem::u8path("reshade-" + id + '.' + type);

	FILE *const file = _wfsopen(path.c_str(), L"wb", SH_DENYNO);
	if (file == nullptr)
		return false;

	const size_t file_size_written = fwrite(data.data(), 1, data.size(), file);
	fclose(file);
	return file_size_written == data.size();
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
		if (filename.wstring().compare(0, 8, L"reshade-") != 0 || (extension != L".i" && extension != L".cso" && extension != L".asm"))
			continue;

		std::filesystem::remove(entry, ec);
	}

	if (ec)
		log::message(log::level::error, "Failed to clear effect cache directory with error code %d!", ec.value());
}

auto reshade::runtime::add_effect_permutation(uint32_t width, uint32_t height, api::format color_format, api::format stencil_format, api::color_space color_space) -> size_t
{
	assert(width != 0 && height != 0);
	assert(color_format != api::format::unknown && stencil_format != api::format::unknown);

	// Handle sRGB and non-sRGB format variants as the same permutation (and use non-sRGB as color format, so that "BUFFER_COLOR_FORMAT" matches 'reshadefx::texture_format' values)
	color_format = api::format_to_default_typed(color_format, 0);

	if (const auto it = std::find_if(_effect_permutations.begin(), _effect_permutations.end(),
			[width, height, color_space, color_format, stencil_format](const effect_permutation &permutation) {
				return permutation.width == width && permutation.height == height && permutation.color_space == color_space && permutation.color_format == color_format && permutation.stencil_format == stencil_format;
			});
		it != _effect_permutations.end())
		return std::distance(_effect_permutations.begin(), it);

	effect_permutation permutation;
	permutation.width = width;
	permutation.height = height;
	permutation.color_space = color_space;
	permutation.color_format = color_format;

	if (!_device->create_resource(
			api::resource_desc(width, height, 1, 1, api::format_to_typeless(color_format), 1, api::memory_heap::gpu_only, api::resource_usage::copy_dest | api::resource_usage::shader_resource),
			nullptr, api::resource_usage::shader_resource, &permutation.color_tex))
	{
		log::message(log::level::error, "Failed to create effect color resource (width = %u, height = %u, format = %u)!", width, height, static_cast<uint32_t>(api::format_to_typeless(color_format)));
		goto exit_failure;
	}

	_device->set_resource_name(permutation.color_tex, "ReShade back buffer");

	if (!_device->create_resource_view(permutation.color_tex, api::resource_usage::shader_resource, api::resource_view_desc(color_format), &permutation.color_srv[0]) ||
		!_device->create_resource_view(permutation.color_tex, api::resource_usage::shader_resource, api::resource_view_desc(api::format_to_default_typed(color_format, 1)), &permutation.color_srv[1]))
	{
		log::message(log::level::error, "Failed to create effect color resource view (format = %u)!", static_cast<uint32_t>(color_format));
		goto exit_failure;
	}

	if (stencil_format != api::format::unknown &&
		_device->create_resource(
			api::resource_desc(width, height, 1, 1, stencil_format, 1, api::memory_heap::gpu_only, api::resource_usage::depth_stencil),
			nullptr, api::resource_usage::depth_stencil_write, &permutation.stencil_tex))
	{
		permutation.stencil_format = stencil_format;

		_device->set_resource_name(permutation.stencil_tex, "ReShade effect stencil");

		if (!_device->create_resource_view(permutation.stencil_tex, api::resource_usage::depth_stencil, api::resource_view_desc(stencil_format), &permutation.stencil_dsv))
		{
			log::message(log::level::error, "Failed to create effect stencil resource view (format = %u)!", static_cast<uint32_t>(stencil_format));
			goto exit_failure;
		}
	}
	else
	{
		log::message(log::level::error, "Failed to create effect stencil resource (width = %u, height = %u, format = %u)!", width, height, static_cast<uint32_t>(stencil_format));

		// Ignore this error, since most effects can still be rendered without stencil
	}

	_effect_permutations.push_back(permutation);
	return _effect_permutations.size() - 1;

exit_failure:
	_device->destroy_resource_view(permutation.color_srv[1]);
	_device->destroy_resource_view(permutation.color_srv[0]);
	_device->destroy_resource(permutation.color_tex);
	_device->destroy_resource(permutation.stencil_tex);

	return std::numeric_limits<size_t>::max();
}

void reshade::runtime::update_effects()
{
	// Delay first load to the first render call to avoid loading while the application is still initializing
	if (_frame_count == 0 && !_no_reload_on_init)
		reload_effects();

	if (!is_loading() && !_is_in_preset_transition && !_reload_required_effects.empty())
	{
		_reload_remaining_effects = 0;

		// Sort list so that all default permutations are reloaded first (since that resets the entire effect), before other permutations
		std::sort(_reload_required_effects.begin(), _reload_required_effects.end(),
			[](const std::pair<size_t, size_t> &lhs, const std::pair<size_t, size_t> &rhs) {
				return lhs.second < rhs.second || (lhs.second == rhs.second && lhs.first < rhs.first);
			});

		for (size_t i = 0; i < _reload_required_effects.size(); ++i)
		{
			const auto [effect_index, permutation_index] = _reload_required_effects[i];

			if (effect_index >= _effects.size())
			{
				reload_effects();
				assert(_reload_required_effects.empty());
				break;
			}

			if (permutation_index == 0)
			{
				if (!reload_effect(effect_index))
					continue;
			}
			else
			{
				// This resize should only happen on the first non-default permutation, before launching threads that can access it
				if (_effects[effect_index].permutations.size() < _effect_permutations.size())
					_effects[effect_index].permutations.resize(_effect_permutations.size());

				_reload_remaining_effects += 1;

				_worker_threads.emplace_back([this, effect_index, permutation_index]() {
						load_effect(_effects[effect_index].source_file, ini_file::load_cache(_current_preset_path), effect_index, permutation_index, true);
					});
			}

			// Force immediate effect initialization of this permutation after reloading
			// This can cause attempts to create an effect that failed to compile, so need to handle that case in 'create_effect' below
			if (std::find(_reload_create_queue.cbegin(), _reload_create_queue.cend(), _reload_required_effects[i]) == _reload_create_queue.cend())
				_reload_create_queue.push_back(_reload_required_effects[i]);
		}

		_reload_required_effects.clear();
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

				if (instance.entry_point_name.empty() && (instance.permutation_index < it->permutations.size() || !instance.generated))
					open_code_editor(instance);
				else
					// Those editors referencing assembly will be updated in a separate step below
					instance.editor.clear_text();
			}
		}
#endif
		return;
	}

	if (_reload_remaining_effects != std::numeric_limits<size_t>::max() || _reload_create_queue.empty())
		return;

	// Pop an effect from the queue
	const auto [effect_index, permutation_index] = _reload_create_queue.back();
	_reload_create_queue.pop_back();
	effect &effect = _effects[effect_index];

	if (!create_effect(effect_index, permutation_index))
	{
		_graphics_queue->wait_idle();

		// Destroy all textures belonging to this effect
		for (texture &tex : _textures)
			if (tex.shared.size() == 1 && tex.shared[0] == effect_index)
				destroy_texture(tex);
		// Disable all techniques belonging to this effect
		for (technique &tech : _techniques)
			if (tech.effect_index == effect_index)
				disable_technique(tech);

		effect.compiled = false;
		_last_reload_successful = false;
	}

#if RESHADE_GUI
	// Update assembly in all code editors after a reload
	for (editor_instance &instance : _editors)
	{
		if (!instance.generated || instance.entry_point_name.empty() || instance.permutation_index != permutation_index || instance.file_path != effect.source_file)
			continue;

		assert(instance.effect_index == effect_index);

		const effect::permutation &permutation = effect.permutations[permutation_index];

		if (permutation.assembly.find(instance.entry_point_name) != permutation.assembly.end())
			open_code_editor(instance);
	}
#endif

#if RESHADE_ADDON
	if (_reload_create_queue.empty())
		invoke_addon_event<addon_event::reshade_reloaded_effects>(this);
#endif
}
void reshade::runtime::render_effects(api::command_list *cmd_list, api::resource_view rtv, api::resource_view rtv_srgb)
{
	// Do not render effects twice in a frame
	if (_effects_rendered_this_frame)
		return;
	_effects_rendered_this_frame = true;

	// Nothing to do here if effects are still loading or disabled globally
	if (is_loading() || _techniques.empty())
		return;
	if (!_effects_enabled && std::all_of(_effects.cbegin(), _effects.cend(), [](const effect &effect) { return !effect.addon; }))
		return;

	// Lock input so it cannot be modified by other threads while we are reading it here
	std::unique_lock<std::recursive_mutex> input_lock;
	if (_input != nullptr
#if RESHADE_ADDON
		&& !_is_in_present_call
#endif
		)
		input_lock = _input->lock();

	// Update special uniform variables
	for (effect &effect : _effects)
	{
		if (!effect.rendering || (!_effects_enabled && !effect.addon))
			continue;

		for (uniform &variable : effect.uniforms)
		{
			switch (variable.special)
			{
			case special_uniform::frame_time:
				set_uniform_value(variable, _last_frame_duration.count() * 1e-6f);
				break;
			case special_uniform::frame_count:
				if (variable.type.is_boolean())
					set_uniform_value(variable, (_frame_count % 2) == 0);
				else
					set_uniform_value(variable, static_cast<unsigned int>(_frame_count % UINT_MAX));
				break;
			case special_uniform::random:
				{
					const int min = variable.annotation_as_int("min", 0, 0);
					const int max = variable.annotation_as_int("max", 0, RAND_MAX);
					set_uniform_value(variable, min + (std::rand() % (std::abs(max - min) + 1)));
				}
				break;
			case special_uniform::ping_pong:
				{
					const float min = variable.annotation_as_float("min", 0, 0.0f);
					const float max = variable.annotation_as_float("max", 0, 1.0f);
					const float step_min = variable.annotation_as_float("step", 0);
					const float step_max = variable.annotation_as_float("step", 1);
					float increment = step_max == 0 ? step_min : (step_min + std::fmod(static_cast<float>(std::rand()), step_max - step_min + 1));
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
				}
				break;
			case special_uniform::date:
				{
					const std::time_t t = std::chrono::system_clock::to_time_t(_current_time);
					struct tm tm; localtime_s(&tm, &t);

					const int value[4] = {
						tm.tm_year + 1900,
						tm.tm_mon + 1,
						tm.tm_mday,
						tm.tm_hour * 3600 + tm.tm_min * 60 + tm.tm_sec
					};
					set_uniform_value(variable, value, 4);
				}
				break;
			case special_uniform::timer:
				{
					const unsigned long long timer_ms = std::chrono::duration_cast<std::chrono::milliseconds>(_last_present_time - _start_time).count();
					set_uniform_value(variable, static_cast<unsigned int>(timer_ms));
				}
				break;
			case special_uniform::key:
				if (_input != nullptr)
				{
					const int keycode = variable.annotation_as_int("keycode");
					if (keycode <= 7 || keycode >= 256)
						break;

					const std::string_view mode = variable.annotation_as_string("mode");
					if (mode == "toggle" || variable.annotation_as_int("toggle"))
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
			case special_uniform::mouse_point:
				if (_input != nullptr)
					set_uniform_value(variable, _input->mouse_position_x(), _input->mouse_position_y());
				break;
			case special_uniform::mouse_delta:
				if (_input != nullptr)
					set_uniform_value(variable, _input->mouse_movement_delta_x(), _input->mouse_movement_delta_y());
				break;
			case special_uniform::mouse_button:
				if (_input != nullptr)
				{
					const int keycode = variable.annotation_as_int("keycode");
					if (keycode < 0 || keycode >= 5)
						break;

					const std::string_view mode = variable.annotation_as_string("mode");
					if (mode == "toggle" || variable.annotation_as_int("toggle"))
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
			case special_uniform::mouse_wheel:
				if (_input != nullptr)
				{
					const float min = variable.annotation_as_float("min");
					const float max = variable.annotation_as_float("max");
					float step = variable.annotation_as_float("step");
					if (step == 0.0f)
						step = 1.0f;

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
				}
				break;
#if RESHADE_GUI
			case special_uniform::overlay_open:
				set_uniform_value(variable, _show_overlay);
				break;
			case special_uniform::overlay_active:
			case special_uniform::overlay_hovered:
				// These are set in 'draw_variable_editor' when overlay is open
				if (!_show_overlay)
					set_uniform_value(variable, 0);
				break;
#endif
			case special_uniform::screenshot:
				set_uniform_value(variable, _should_save_screenshot);
				break;
			}
		}
	}

	if (rtv == 0)
		return;
	if (rtv_srgb == 0)
		rtv_srgb = rtv;

	const api::resource back_buffer_resource = _device->get_resource_from_view(rtv);

	size_t permutation_index = 0;
#if RESHADE_ADDON
	if (!_is_in_present_call &&
		// Special case for when add-on passed in the back buffer, which behaves as if this was called from within present, using the default permutation
		back_buffer_resource != get_current_back_buffer())
	{
		const api::resource_desc back_buffer_desc = _device->get_resource_desc(back_buffer_resource);
		if (back_buffer_desc.texture.samples > 1)
			return; // Multisampled render targets are not supported

		api::format color_format = back_buffer_desc.texture.format;
		if (api::format_to_typeless(color_format) == color_format)
			color_format = _device->get_resource_view_desc(rtv).format;

		// Ensure dimensions and format of the effect color resource matches that of the input back buffer resource (so that the copy to the effect color resource succeeds)
		// Changing dimensions or format can cause effects to be reloaded, in which case need to wait for that to finish before rendering
		permutation_index = add_effect_permutation(back_buffer_desc.texture.width, back_buffer_desc.texture.height, color_format, _effect_permutations[0].stencil_format, api::color_space::unknown);
		if (permutation_index == std::numeric_limits<size_t>::max())
			return;
	}

	if (!_is_in_present_call)
		api::capture_state(cmd_list, _app_state);

	invoke_addon_event<addon_event::reshade_begin_effects>(this, cmd_list, rtv, rtv_srgb);
#endif

#ifndef NDEBUG
	cmd_list->begin_debug_event("ReShade effects");
#endif

	// Render all enabled techniques
	for (size_t technique_index : _technique_sorting)
	{
		technique &tech = _techniques[technique_index];

		const size_t effect_index = tech.effect_index;

		if (!tech.enabled || (_should_save_screenshot && !tech.enabled_in_screenshot) || (!_effects_enabled && !_effects[effect_index].addon))
			continue;

		if (permutation_index >= tech.permutations.size() ||
			(!tech.permutations[permutation_index].created && _effects[effect_index].permutations[permutation_index].cso.empty()))
		{
			if (std::find(_reload_required_effects.begin(), _reload_required_effects.end(), std::make_pair(effect_index, permutation_index)) == _reload_required_effects.end())
				_reload_required_effects.emplace_back(effect_index, permutation_index);
			continue;
		}

		render_technique(tech, cmd_list, back_buffer_resource, rtv, rtv_srgb, permutation_index);

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

	if (!_is_in_present_call)
		api::apply_state(cmd_list, _app_state);
#endif
}
void reshade::runtime::render_technique(technique &tech, api::command_list *cmd_list, api::resource back_buffer_resource, api::resource_view back_buffer_rtv, api::resource_view back_buffer_rtv_srgb, size_t permutation_index)
{
	const effect &effect = _effects[tech.effect_index];
	const effect::permutation &permutation = effect.permutations[permutation_index];

#ifndef NDEBUG
	cmd_list->begin_debug_event(tech.name.c_str());
#endif

#if RESHADE_GUI
	uint32_t query_base_index = 0;
	const bool gather_gpu_statistics = _gather_gpu_statistics && _timestamp_frequency != 0 && effect.query_heap != 0 && permutation_index == 0;

	if (gather_gpu_statistics)
	{
		const uint32_t query_count = static_cast<uint32_t>((1 + tech.permutations[0].passes.size()) * 2);
		query_base_index = tech.query_base_index + (_frame_count % 4) * query_count;

		// Evaluate queries from oldest frame in queue
		if (temp_mem<uint64_t> timestamps(query_count);
			_device->get_query_heap_results(effect.query_heap, query_base_index, query_count, timestamps.p, sizeof(uint64_t)))
		{
			const uint64_t tech_duration = timestamps[1] - timestamps[0];
			tech.average_gpu_duration.append(tech_duration * 1'000'000'000ull / _timestamp_frequency);

			for (size_t pass_index = 0; pass_index < tech.permutations[0].passes.size(); ++pass_index)
			{
				const uint64_t pass_duration = timestamps[2 + pass_index * 2 + 1] - timestamps[2 + pass_index * 2];
				tech.permutations[0].passes[pass_index].average_gpu_duration.append(pass_duration * 1'000'000'000ull / _timestamp_frequency);
			}
		}

		cmd_list->end_query(effect.query_heap, api::query_type::timestamp, query_base_index);
	}

	const std::chrono::high_resolution_clock::time_point time_technique_started = std::chrono::high_resolution_clock::now();
#endif

	// Update shader constants
	if (void *mapped_uniform_data;
		effect.cb != 0 && _device->map_buffer_region(effect.cb, 0, effect.uniform_data_storage.size(), api::map_access::write_discard, &mapped_uniform_data))
	{
		std::memcpy(mapped_uniform_data, effect.uniform_data_storage.data(), effect.uniform_data_storage.size());
		_device->unmap_buffer_region(effect.cb);
	}
	else if (_device->get_api() == api::device_api::d3d9)
	{
		cmd_list->push_constants(api::shader_stage::all, permutation.layout, 0, 0, static_cast<uint32_t>(effect.uniform_data_storage.size() / 4), effect.uniform_data_storage.data());
	}

	const bool sampler_with_resource_view = _device->check_capability(api::device_caps::sampler_with_resource_view);

	bool is_effect_stencil_cleared = false;
	bool needs_implicit_back_buffer_copy = true; // First pass always needs the back buffer updated

	for (size_t pass_index = 0; pass_index < tech.permutations[permutation_index].passes.size(); ++pass_index)
	{
		if (needs_implicit_back_buffer_copy)
		{
			// Save back buffer of previous pass
			const api::resource resources[2] = { back_buffer_resource, _effect_permutations[permutation_index].color_tex};
			const api::resource_usage state_old[2] = { api::resource_usage::render_target, api::resource_usage::shader_resource };
			const api::resource_usage state_new[2] = { api::resource_usage::copy_source, api::resource_usage::copy_dest };

			cmd_list->barrier(2, resources, state_old, state_new);
			cmd_list->copy_texture_region(back_buffer_resource, 0, nullptr, _effect_permutations[permutation_index].color_tex, 0, nullptr);
			cmd_list->barrier(2, resources, state_new, state_old);
		}

		const technique::pass &pass = tech.permutations[permutation_index].passes[pass_index];

#ifndef NDEBUG
		cmd_list->begin_debug_event((pass.name.empty() ? "Pass " + std::to_string(pass_index) : pass.name).c_str());
#endif

#if RESHADE_GUI
		if (gather_gpu_statistics)
			cmd_list->end_query(effect.query_heap, api::query_type::timestamp, query_base_index + static_cast<uint32_t>((1 + pass_index) * 2));
#endif

		const uint32_t num_barriers = static_cast<uint32_t>(pass.modified_resources.size());

		if (!pass.cs_entry_point.empty())
		{
			// Compute shaders do not write to the back buffer, so no update necessary
			needs_implicit_back_buffer_copy = false;

			cmd_list->bind_pipeline(api::pipeline_stage::all_compute, pass.pipeline);

			temp_mem<api::resource_usage> state_old, state_new;
			std::fill_n(state_old.p, num_barriers, api::resource_usage::shader_resource);
			std::fill_n(state_new.p, num_barriers, api::resource_usage::unordered_access);
			cmd_list->barrier(num_barriers, pass.modified_resources.data(), state_old.p, state_new.p);

			// Reset bindings on every pass (since they get invalidated by the call to 'generate_mipmaps' below)
			if (effect.cb != 0)
				cmd_list->bind_descriptor_table(api::shader_stage::all_compute, permutation.layout, 0, permutation.cb_table);
			if (permutation.sampler_table != 0)
				assert(!sampler_with_resource_view),
				cmd_list->bind_descriptor_table(api::shader_stage::all_compute, permutation.layout, 1, permutation.sampler_table);
			if (!pass.texture_bindings.empty())
				cmd_list->bind_descriptor_table(api::shader_stage::all_compute, permutation.layout, sampler_with_resource_view ? 1 : 2, pass.texture_table);
			if (!pass.storage_bindings.empty())
				cmd_list->bind_descriptor_table(api::shader_stage::all_compute, permutation.layout, sampler_with_resource_view ? 2 : 3, pass.storage_table);

			cmd_list->dispatch(pass.viewport_width, pass.viewport_height, pass.viewport_dispatch_z);

			cmd_list->barrier(num_barriers, pass.modified_resources.data(), state_new.p, state_old.p);
		}
		else
		{
			cmd_list->bind_pipeline(api::pipeline_stage::all_graphics, pass.pipeline);

			// Transition resource state for render targets
			temp_mem<api::resource_usage> state_old, state_new;
			std::fill_n(state_old.p, num_barriers, api::resource_usage::shader_resource);
			std::fill_n(state_new.p, num_barriers, api::resource_usage::render_target);
			cmd_list->barrier(num_barriers, pass.modified_resources.data(), state_old.p, state_new.p);

			// Setup render targets
			uint32_t render_target_count = 0;
			api::render_pass_depth_stencil_desc depth_stencil = {};
			api::render_pass_render_target_desc render_target[8] = {};

			if (pass.render_target_names[0].empty())
			{
				needs_implicit_back_buffer_copy = true;

				render_target[0].view = pass.srgb_write_enable ? back_buffer_rtv_srgb : back_buffer_rtv;
				render_target_count = 1;
			}
			else
			{
				needs_implicit_back_buffer_copy = false;

				for (int i = 0; i < 8 && pass.render_target_views[i] != 0; ++i, ++render_target_count)
					render_target[i].view = pass.render_target_views[i];
			}

			if (pass.clear_render_targets)
			{
				for (int i = 0; i < 8; ++i)
					render_target[i].load_op = api::render_pass_load_op::clear;
			}

			if (pass.stencil_enable &&
				pass.viewport_width == _effect_permutations[permutation_index].width &&
				pass.viewport_height == _effect_permutations[permutation_index].height)
			{
				depth_stencil.view = _effect_permutations[permutation_index].stencil_dsv;

				// First pass to use the stencil buffer should clear it
				if (!is_effect_stencil_cleared)
					depth_stencil.stencil_load_op = api::render_pass_load_op::clear, is_effect_stencil_cleared = true;
			}

			cmd_list->begin_render_pass(render_target_count, render_target, depth_stencil.view != 0 ? &depth_stencil : nullptr);

			// Reset bindings on every pass (since they get invalidated by the call to 'generate_mipmaps' below)
			if (effect.cb != 0)
				cmd_list->bind_descriptor_table(api::shader_stage::all_graphics, permutation.layout, 0, permutation.cb_table);
			if (permutation.sampler_table != 0)
				assert(!sampler_with_resource_view),
				cmd_list->bind_descriptor_table(api::shader_stage::all_graphics, permutation.layout, 1, permutation.sampler_table);
			// Setup shader resources after binding render targets, to ensure any OM bindings by the application are unset at this point (e.g. a depth buffer that was bound to the OM and is now bound as shader resource)
			if (!pass.texture_bindings.empty())
				cmd_list->bind_descriptor_table(api::shader_stage::all_graphics, permutation.layout, sampler_with_resource_view ? 1 : 2, pass.texture_table);

			const api::viewport viewport = {
				0.0f, 0.0f,
				static_cast<float>(pass.viewport_width),
				static_cast<float>(pass.viewport_height),
				0.0f, 1.0f
			};
			cmd_list->bind_viewports(0, 1, &viewport);

			const api::rect scissor_rect = {
				0, 0,
				static_cast<int32_t>(pass.viewport_width),
				static_cast<int32_t>(pass.viewport_height)
			};
			cmd_list->bind_scissor_rects(0, 1, &scissor_rect);

			if (_device->get_api() == api::device_api::d3d9)
			{
				// Set __TEXEL_SIZE__ constant (see effect_codegen_hlsl.cpp)
				const float texel_size[4] = {
					-1.0f / pass.viewport_width,
					 1.0f / pass.viewport_height
				};
				cmd_list->push_constants(api::shader_stage::vertex, permutation.layout, 0, 255 * 4, 4, texel_size);

				// Set SEMANTIC_PIXEL_SIZE constants (see effect_codegen_hlsl.cpp)
				for (const reshadefx::texture &tex : permutation.module.textures)
				{
					if (tex.semantic.empty())
						continue;

					if (tex.semantic == "COLOR")
					{
						const float pixel_size[4] = {
							1.0f / _effect_permutations[permutation_index].width,
							1.0f / _effect_permutations[permutation_index].height
						};

						cmd_list->push_constants(api::shader_stage::vertex | api::shader_stage::pixel, permutation.layout, 0, tex.semantic_binding * 4, 4, pixel_size);
					}
					else if (const auto it = _texture_semantic_bindings.find(tex.semantic); it != _texture_semantic_bindings.end())
					{
						const api::resource_desc desc = _device->get_resource_desc(_device->get_resource_from_view(it->second.first));

						const float pixel_size[4] = {
							1.0f / desc.texture.width,
							1.0f / desc.texture.height
						};

						cmd_list->push_constants(api::shader_stage::vertex | api::shader_stage::pixel, permutation.layout, 0, tex.semantic_binding * 4, 4, pixel_size);
					}
				}
			}

			// Draw primitives
			cmd_list->draw(pass.num_vertices, 1, 0, 0);

			cmd_list->end_render_pass();

			// Transition resource state back to shader access
			cmd_list->barrier(num_barriers, pass.modified_resources.data(), state_new.p, state_old.p);
		}

#if RESHADE_GUI
		if (gather_gpu_statistics)
			cmd_list->end_query(effect.query_heap, api::query_type::timestamp, query_base_index + static_cast<uint32_t>((1 + pass_index) * 2) + 1);
#endif

#ifndef NDEBUG
		cmd_list->end_debug_event();
#endif

		// Generate mipmaps for modified resources
		for (const api::resource_view modified_texture : pass.generate_mipmap_views)
			cmd_list->generate_mipmaps(modified_texture);
	}

#if RESHADE_GUI
	const std::chrono::high_resolution_clock::time_point time_technique_finished = std::chrono::high_resolution_clock::now();

	tech.average_cpu_duration.append(std::chrono::duration_cast<std::chrono::nanoseconds>(time_technique_finished - time_technique_started).count());

	if (gather_gpu_statistics)
		cmd_list->end_query(effect.query_heap, api::query_type::timestamp, query_base_index + 1);
#endif

#ifndef NDEBUG
	cmd_list->end_debug_event();
#endif

#if RESHADE_ADDON
	invoke_addon_event<addon_event::reshade_render_technique>(const_cast<runtime *>(this), api::effect_technique { reinterpret_cast<uintptr_t>(&tech) }, cmd_list, back_buffer_rtv, back_buffer_rtv_srgb);
#endif
}

void reshade::runtime::save_texture(const texture &tex)
{
	if (tex.type == reshadefx::texture_type::texture_3d)
	{
		log::message(log::level::error, "Texture saving is not supported for 3D textures!");
		return;
	}

	std::string screenshot_name = tex.unique_name;
	switch (_screenshot_format)
	{
	case 0:
		screenshot_name += ".bmp";
		break;
	case 1:
		screenshot_name += ".png";
		break;
	case 2:
		screenshot_name += ".jpg";
		break;
	case 3:
		screenshot_name += ".jxl";
		break;
	default:
		return;
	}

	const std::filesystem::path screenshot_path = g_reshade_base_path / _screenshot_path / std::filesystem::u8path(screenshot_name);

	_last_screenshot_save_successful = true;

	if (std::vector<uint8_t> pixels(static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height) * 4);
		get_texture_data(tex.resource, api::resource_usage::shader_resource, pixels.data(), api::format::r8g8b8a8_unorm))
	{
		_worker_threads.emplace_back([this, screenshot_path, pixels = std::move(pixels), width = tex.width, height = tex.height]() mutable {
			// Default to a save failure unless it is reported to succeed below
			bool save_success = false;

			if (FILE *const file = _wfsopen(screenshot_path.c_str(), L"wb", SH_DENYNO))
			{
				const auto write_callback = [](void *context, void *data, int size) {
					fwrite(data, 1, size, static_cast<FILE *>(context));
				};

				switch (_screenshot_format)
				{
				case 0:
					save_success = stbi_write_bmp_to_func(write_callback, file, width, height, 4, pixels.data()) != 0;
					break;
				case 1:
#if 1
					if (std::vector<uint8_t> encoded_data;
						fpng::fpng_encode_image_to_memory(pixels.data(), width, height, 4, encoded_data))
						save_success = fwrite(encoded_data.data(), 1, encoded_data.size(), file) == encoded_data.size();
#else
					save_success = stbi_write_png_to_func(write_callback, file, width, height, 4, pixels.data(), 0) != 0;
#endif
					break;
				case 2:
					save_success = stbi_write_jpg_to_func(write_callback, file, width, height, 4, pixels.data(), _screenshot_jpeg_quality) != 0;
					break;
				case 3:
					JxlColorEncoding color_encoding;
					color_encoding.color_space = JXL_COLOR_SPACE_RGB;
					color_encoding.white_point = JXL_WHITE_POINT_D65;
					color_encoding.primaries = JXL_PRIMARIES_SRGB;
					color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
					color_encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
					color_encoding.is_float = false;

					uint8_t *encoded_data = nullptr;
					const size_t encoded_size = JxlSimpleLosslessEncode(
						pixels.data(),
						width,
						static_cast<size_t>(width) * 4,
						height,
						4,
						/* bitdepth = */ 8,
						/* big_endian = */ false,
						/* effort = */ 2,
						&encoded_data,
						nullptr,
						[](void *, void *opaque, void fun(void *, size_t), size_t count) {
							const size_t num_splits = std::min(count, static_cast<size_t>(std::thread::hardware_concurrency()));
							if (num_splits == 1)
							{
								for (size_t i = 0; i < count; ++i)
									fun(opaque, i);
								return;
							}
							std::vector<std::thread> worker_threads;
							for (size_t n = 0; n < num_splits; ++n)
								worker_threads.emplace_back([count, opaque, fun, num_splits, n]() {
									for (size_t i = 0; i < count; ++i)
										if (i * num_splits / count == n)
											fun(opaque, i);
								});
							for (std::thread &thread : worker_threads)
								thread.join();
						},
						color_encoding);

					if (encoded_data && encoded_size > 0)
					{
						save_success = fwrite(encoded_data, 1, encoded_size, file) == encoded_size;
						free(encoded_data);
					}
					break;
				}

				if (ferror(file))
					save_success = false;

				fclose(file);
			}

			if (_last_screenshot_save_successful)
			{
				_last_screenshot_time = std::chrono::high_resolution_clock::now();
				_last_screenshot_file = screenshot_path;
				_last_screenshot_save_successful = save_success;
			}
		});
	}
}
void reshade::runtime::update_texture(texture &tex, uint32_t width, uint32_t height, uint32_t depth, const void *pixels)
{
	if (tex.depth != depth || (tex.depth != 1 && (tex.width != width || tex.height != height)))
	{
		log::message(log::level::error, "Resizing image data is not supported for 3D textures like '%s'.", tex.unique_name.c_str());
		return;
	}

	uint32_t pixel_size;
	stbir_datatype data_type;
	stbir_pixel_layout pixel_layout;
	switch (tex.format)
	{
	case reshadefx::texture_format::r8:
		pixel_size = 1 * 1;
		data_type = STBIR_TYPE_UINT8;
		pixel_layout = STBIR_1CHANNEL;
		break;
	case reshadefx::texture_format::r32f:
		pixel_size = 4 * 1;
		data_type = STBIR_TYPE_FLOAT;
		pixel_layout = STBIR_1CHANNEL;
		break;
	case reshadefx::texture_format::rg8:
		pixel_size = 1 * 2;
		data_type = STBIR_TYPE_UINT8;
		pixel_layout = STBIR_2CHANNEL;
		break;
	case reshadefx::texture_format::rg16:
		pixel_size = 2 * 2;
		data_type = STBIR_TYPE_UINT16;
		pixel_layout = STBIR_2CHANNEL;
		break;
	case reshadefx::texture_format::rg16f:
		pixel_size = 2 * 2;
		data_type = STBIR_TYPE_HALF_FLOAT;
		pixel_layout = STBIR_2CHANNEL;
		break;
	case reshadefx::texture_format::rg32f:
		pixel_size = 4 * 2;
		data_type = STBIR_TYPE_FLOAT;
		pixel_layout = STBIR_2CHANNEL;
		break;
	case reshadefx::texture_format::rgba8:
	case reshadefx::texture_format::rgb10a2:
		pixel_size = 1 * 4;
		data_type = STBIR_TYPE_UINT8;
		pixel_layout = STBIR_RGBA;
		break;
	case reshadefx::texture_format::rgba16:
		pixel_size = 2 * 4;
		data_type = STBIR_TYPE_UINT16;
		pixel_layout = STBIR_RGBA;
		break;
	case reshadefx::texture_format::rgba16f:
		pixel_size = 2 * 4;
		data_type = STBIR_TYPE_HALF_FLOAT;
		pixel_layout = STBIR_RGBA;
		break;
	case reshadefx::texture_format::rgba32f:
		pixel_size = 4 * 4;
		data_type = STBIR_TYPE_FLOAT;
		pixel_layout = STBIR_RGBA;
		break;
	default:
		return;
	}

	void *upload_data = const_cast<void *>(pixels);

	// Need to potentially resize image data to the texture dimensions
	std::vector<uint8_t> resized;
	if (tex.width != width || tex.height != height)
	{
		log::message(log::level::info, "Resizing image data for texture '%s' from %ux%u to %ux%u.", tex.unique_name.c_str(), width, height, tex.width, tex.height);

		resized.resize(static_cast<size_t>(tex.width) * static_cast<size_t>(tex.height) * static_cast<size_t>(tex.depth) * static_cast<size_t>(pixel_size));

		upload_data = stbir_resize(pixels, width, height, 0, resized.data(), tex.width, tex.height, 0, pixel_layout, data_type, STBIR_EDGE_CLAMP, STBIR_FILTER_DEFAULT);
	}

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
	cmd_list->barrier(tex.resource, api::resource_usage::shader_resource, api::resource_usage::copy_dest);
	cmd_list->update_texture_region({ upload_data, tex.width * pixel_size, tex.width * tex.height * pixel_size }, tex.resource, 0);
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

	const reshadefx::constant zero = {};

	// Need to use typed setters, to ensure values are properly forced to floating point in D3D9
	for (size_t i = 0, array_length = (variable.type.is_array() ? variable.type.array_length : 1u); i < array_length; ++i)
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

static bool force_floating_point_value(const reshadefx::type &type, uint32_t renderer_id)
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

	const size_t array_length = (variable.type.is_array() ? variable.type.array_length : 1u);
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
	if (!is_loading() && invoke_addon_event<addon_event::reshade_set_uniform_value>(this, api::effect_uniform_variable { reinterpret_cast<uintptr_t>(&variable) }, data, size))
		return;
#endif

	size = std::min(size, static_cast<size_t>(variable.size));
	assert(data != nullptr && (size % 4) == 0);

	std::vector<uint8_t> &data_storage = _effects[variable.effect_index].uniform_data_storage;
	assert(variable.offset + size <= data_storage.size());

	const size_t array_length = (variable.type.is_array() ? variable.type.array_length : 1u);
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

static std::string expand_macro_string(const std::string &input, std::vector<std::pair<std::string, std::string>> macros, std::chrono::system_clock::time_point now)
{
	const auto now_seconds = std::chrono::time_point_cast<std::chrono::seconds>(now);

	char timestamp[21];
	const std::time_t t = std::chrono::system_clock::to_time_t(now_seconds);
	struct tm tm; localtime_s(&tm, &t);

	std::snprintf(timestamp, std::size(timestamp), "%.4d-%.2d-%.2d", tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
	macros.emplace_back("Date", timestamp);
	std::snprintf(timestamp, std::size(timestamp), "%.4d", tm.tm_year + 1900);
	macros.emplace_back("DateYear", timestamp);
	macros.emplace_back("Year", timestamp);
	std::snprintf(timestamp, std::size(timestamp), "%.2d", tm.tm_mon + 1);
	macros.emplace_back("DateMonth", timestamp);
	macros.emplace_back("Month", timestamp);
	std::snprintf(timestamp, std::size(timestamp), "%.2d", tm.tm_mday);
	macros.emplace_back("DateDay", timestamp);
	macros.emplace_back("Day", timestamp);

	std::snprintf(timestamp, std::size(timestamp), "%.2d-%.2d-%.2d", tm.tm_hour, tm.tm_min, tm.tm_sec);
	macros.emplace_back("Time", timestamp);
	std::snprintf(timestamp, std::size(timestamp), "%.2d", tm.tm_hour);
	macros.emplace_back("TimeHour", timestamp);
	macros.emplace_back("Hour", timestamp);
	std::snprintf(timestamp, std::size(timestamp), "%.2d", tm.tm_min);
	macros.emplace_back("TimeMinute", timestamp);
	macros.emplace_back("Minute", timestamp);
	std::snprintf(timestamp, std::size(timestamp), "%.2d", tm.tm_sec);
	macros.emplace_back("TimeSecond", timestamp);
	macros.emplace_back("Second", timestamp);
	std::snprintf(timestamp, std::size(timestamp), "%.3lld", std::chrono::duration_cast<std::chrono::milliseconds>(now - now_seconds).count());
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

void reshade::runtime::save_screenshot(const char *postfix_in)
{
	std::string postfix;
	if (postfix_in != nullptr)
		postfix = postfix_in;

	const unsigned int screenshot_count = _screenshot_count;
	// Use PNG or JPEG XL for HDR (no tonemapping is implemented, so this is the only way to capture a screenshot in HDR)
	const unsigned int screenshot_format =
		(_back_buffer_format == api::format::r16g16b16a16_float || _back_buffer_color_space == api::color_space::hdr10_pq) ? (_screenshot_format == 3 ? 5 : 4) : _screenshot_format;

	std::string screenshot_name = expand_macro_string(_screenshot_name, {
		{ "AppName", g_target_executable_path.stem().u8string() },
		{ "PresetName", _current_preset_path.stem().u8string() },
		{ "BeforeAfter", postfix },
		{ "Count", std::to_string(screenshot_count) }
	}, _current_time);

	if (!postfix.empty() && _screenshot_name.find("%BeforeAfter%") == std::string::npos)
	{
		screenshot_name += ' ';
		screenshot_name += postfix;
	}

	switch (screenshot_format)
	{
	case 0:
		screenshot_name += ".bmp";
		break;
	case 1:
	case 4:
		screenshot_name += ".png";
		break;
	case 2:
		screenshot_name += ".jpg";
		break;
	case 3:
	case 5:
		screenshot_name += ".jxl";
		break;
	default:
		return;
	}

	const std::filesystem::path screenshot_path = g_reshade_base_path / _screenshot_path / std::filesystem::u8path(screenshot_name).lexically_normal();

	log::message(log::level::info, "Saving screenshot to '%s'.", screenshot_path.u8string().c_str());

	_last_screenshot_save_successful = true;

	if (std::vector<uint8_t> pixels(static_cast<size_t>(_width) * static_cast<size_t>(_height) * (screenshot_format >= 4 ? 6 : 4));
		get_texture_data(
			_back_buffer_resolved != 0 ? _back_buffer_resolved : _swapchain->get_current_back_buffer(),
			_back_buffer_resolved != 0 ? api::resource_usage::render_target : api::resource_usage::present,
			pixels.data(),
			screenshot_format >= 4 ? (_back_buffer_format == api::format::r16g16b16a16_float ? api::format::r16g16b16_float : api::format::r16g16b16_unorm) : api::format::r8g8b8a8_unorm))
	{
		const bool include_preset =
			_screenshot_include_preset &&
			postfix != "Before" && postfix != "Overlay" &&
			ini_file::flush_cache(_current_preset_path);

		// Play screenshot sound
		if (!_screenshot_sound_path.empty())
			utils::play_sound_async(g_reshade_base_path / _screenshot_sound_path);

		_worker_threads.emplace_back([this, screenshot_count, screenshot_format, screenshot_path, postfix, pixels = std::move(pixels), include_preset]() mutable {
			// Remove alpha channel
			int comp = 4;
			if (screenshot_format >= 4)
			{
				comp = 3;
			}
			else if (_screenshot_clear_alpha)
			{
				comp = 3;
				for (size_t i = 0; i < static_cast<size_t>(_width) * static_cast<size_t>(_height); ++i)
					*reinterpret_cast<uint32_t *>(pixels.data() + 3 * i) = *reinterpret_cast<const uint32_t *>(pixels.data() + 4 * i);
			}

			// Create screenshot directory if it does not exist
			std::error_code ec;
			_screenshot_directory_creation_successful = true;
			if (!std::filesystem::exists(screenshot_path.parent_path(), ec))
				if (!(_screenshot_directory_creation_successful = std::filesystem::create_directories(screenshot_path.parent_path(), ec)))
					log::message(log::level::error, "Failed to create screenshot directory '%s' with error code %d!", screenshot_path.parent_path().u8string().c_str(), ec.value());

			// Default to a save failure unless it is reported to succeed below
			bool save_success = false;

			if (FILE *const file = _wfsopen(screenshot_path.c_str(), L"wb", SH_DENYNO))
			{
				const auto write_callback = [](void *context, void *data, int size) {
					fwrite(data, 1, size, static_cast<FILE *>(context));
				};

				switch (screenshot_format)
				{
				case 0:
					save_success = stbi_write_bmp_to_func(write_callback, file, _width, _height, comp, pixels.data()) != 0;
					break;
				case 1:
#if 1
					if (std::vector<uint8_t> encoded_data;
						fpng::fpng_encode_image_to_memory(pixels.data(), _width, _height, comp, encoded_data))
						save_success = fwrite(encoded_data.data(), 1, encoded_data.size(), file) == encoded_data.size();
#else
					save_success = stbi_write_png_to_func(write_callback, file, _width, _height, comp, pixels.data(), 0) != 0;
#endif
					break;
				case 2:
					save_success = stbi_write_jpg_to_func(write_callback, file, _width, _height, comp, pixels.data(), _screenshot_jpeg_quality) != 0;
					break;
				case 4: // HDR PNG
					if (_back_buffer_format == api::format::r16g16b16a16_float)
					{
						if (!fpng::fpng_cpu_supports_sse41())
						{
							// Technically requires F16C instruction set, not just SSE4.1
							save_success = false;
							break;
						}

						for (size_t i = 0; i < static_cast<size_t>(_width) * static_cast<size_t>(_height); ++i)
						{
							uint16_t *const pixel = reinterpret_cast<uint16_t *>(pixels.data()) + i * 3;
							alignas(16) uint16_t result[4] = { pixel[0], pixel[1], pixel[2] };

							// Convert 16-bit floating point values to 32-bit floating point
							auto rgba_float_srgb = _mm_cvtph_ps(_mm_loadl_epi64(reinterpret_cast<const __m128i *>(result)));

							// Convert BT.709/sRGB to BT.2020 primaries
							auto rgba_float_bt2100 = _mm_max_ps(_mm_setzero_ps(),
								_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(rgba_float_srgb, rgba_float_srgb, 0b00000000), _mm_setr_ps(0.627403914928436279296875f,     0.069097287952899932861328125f,    0.01639143936336040496826171875f, 0.0f)),
								_mm_add_ps(_mm_mul_ps(_mm_shuffle_ps(rgba_float_srgb, rgba_float_srgb, 0b01010101), _mm_setr_ps(0.3292830288410186767578125f,    0.9195404052734375f,               0.08801330626010894775390625f,    0.0f)),
								           _mm_mul_ps(_mm_shuffle_ps(rgba_float_srgb, rgba_float_srgb, 0b10101010), _mm_setr_ps(0.0433130674064159393310546875f, 0.011362315155565738677978515625f, 0.895595252513885498046875f,      0.0f)))));

							// Convert linear to PQ
							// PQ constants as per Rec. ITU-R BT.2100-3 Table 4
							const float PQ_m1 = 0.1593017578125f;
							const float PQ_m2 = 78.84375f;
							const float PQ_c1 = 0.8359375f;
							const float PQ_c2 = 18.8515625f;
							const float PQ_c3 = 18.6875f;

							auto rgba_float_bt2100_pq = _mm_div_ps(rgba_float_bt2100, _mm_set_ps1(125.0f));
							alignas(16) float temp[4];
							_mm_store_ps(temp, rgba_float_bt2100_pq);
							rgba_float_bt2100_pq = _mm_setr_ps(std::powf(temp[0], PQ_m1), std::powf(temp[1], PQ_m1), std::powf(temp[2], PQ_m1), 0.0f);
							rgba_float_bt2100_pq = _mm_div_ps(_mm_add_ps(_mm_mul_ps(_mm_set_ps1(PQ_c2), rgba_float_bt2100_pq), _mm_set_ps1(PQ_c1)), _mm_add_ps(_mm_mul_ps(_mm_set_ps1(PQ_c3), rgba_float_bt2100_pq), _mm_set_ps1(1.0f)));
							_mm_store_ps(temp, rgba_float_bt2100_pq);
							rgba_float_bt2100_pq = _mm_setr_ps(std::powf(temp[0], PQ_m2), std::powf(temp[1], PQ_m2), std::powf(temp[2], PQ_m2), 0.0f);

							// Convert to integers and pack into 16-bit range
							_mm_storel_epi64(reinterpret_cast<__m128i *>(result), _mm_packus_epi32(_mm_cvtps_epi32(_mm_mul_ps(rgba_float_bt2100_pq, _mm_set_ps1(65536.0f))), _mm_setzero_si128()));

							pixel[0] = result[0];
							pixel[1] = result[1];
							pixel[2] = result[2];
						}
					}

					save_success = stbi_write_hdr_png_to_func(
						write_callback,
						file,
						_width,
						_height,
						comp,
						reinterpret_cast<uint16_t *>(pixels.data()),
						0,
						static_cast<unsigned char>(JXL_PRIMARIES_2100),
						static_cast<unsigned char>(_back_buffer_color_space == api::color_space::hdr10_hlg ? JXL_TRANSFER_FUNCTION_HLG : JXL_TRANSFER_FUNCTION_PQ)) != 0;
					break;
				case 3:
				case 5: // HDR JPEG XL
					JxlColorEncoding color_encoding;
					color_encoding.color_space = JXL_COLOR_SPACE_RGB;
					color_encoding.white_point = JXL_WHITE_POINT_D65;
					color_encoding.rendering_intent = JXL_RENDERING_INTENT_RELATIVE;
					color_encoding.is_float = _back_buffer_format == api::format::r16g16b16a16_float;

					switch (_back_buffer_color_space)
					{
					default:
					case api::color_space::srgb:
						color_encoding.primaries = JXL_PRIMARIES_SRGB;
						color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_SRGB;
						break;
					case api::color_space::scrgb:
						color_encoding.primaries = JXL_PRIMARIES_SRGB;
						color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_LINEAR;
						break;
					case api::color_space::hdr10_pq:
						color_encoding.primaries = JXL_PRIMARIES_2100;
						color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_PQ;
						break;
					case api::color_space::hdr10_hlg:
						color_encoding.primaries = JXL_PRIMARIES_2100;
						color_encoding.transfer_function = JXL_TRANSFER_FUNCTION_HLG;
						break;
					}

					uint8_t *encoded_data = nullptr;
					const size_t encoded_size = JxlSimpleLosslessEncode(
						pixels.data(),
						_width,
						static_cast<size_t>(_width) * comp * (screenshot_format >= 4 ? 2 : 1),
						_height,
						comp,
						screenshot_format >= 4 ? 16 : 8,
						/* big_endian = */ false,
						/* effort = */ 2,
						&encoded_data,
						nullptr,
						[](void *, void *opaque, void fun(void *, size_t), size_t count) {
							const size_t num_splits = std::min(count, static_cast<size_t>(std::thread::hardware_concurrency()));
							if (num_splits == 1)
							{
								for (size_t i = 0; i < count; ++i)
									fun(opaque, i);
								return;
							}
							std::vector<std::thread> worker_threads;
							for (size_t n = 0; n < num_splits; ++n)
								worker_threads.emplace_back([count, opaque, fun, num_splits, n]() {
									for (size_t i = 0; i < count; ++i)
										if (i * num_splits / count == n)
											fun(opaque, i);
								});
							for (std::thread &thread : worker_threads)
								thread.join();
						},
						color_encoding);

					if (encoded_data && encoded_size > 0)
					{
						save_success = fwrite(encoded_data, 1, encoded_size, file) == encoded_size;
						free(encoded_data);
					}
					break;
				}

				if (ferror(file))
					save_success = false;

				fclose(file);
			}

			if (save_success)
			{
				execute_screenshot_post_save_command(screenshot_path, screenshot_count, postfix);

				if (include_preset)
				{
					std::filesystem::path screenshot_preset_path = screenshot_path;
					screenshot_preset_path.replace_extension(L".ini");

					// Preset was flushed to disk, so can just copy it over to the new location
					if (!std::filesystem::copy_file(_current_preset_path, screenshot_preset_path, std::filesystem::copy_options::overwrite_existing, ec))
						log::message(log::level::error, "Failed to copy preset file for screenshot to '%s' with error code %d!", screenshot_preset_path.u8string().c_str(), ec.value());
				}

#if RESHADE_ADDON
				invoke_addon_event<addon_event::reshade_screenshot>(this, screenshot_path.u8string().c_str());
#endif
			}
			else
			{
				log::message(log::level::error, "Failed to write screenshot to '%s'!", screenshot_path.u8string().c_str());
			}

			if (_last_screenshot_save_successful)
			{
				_last_screenshot_time = std::chrono::high_resolution_clock::now();
				_last_screenshot_file = screenshot_path;
				_last_screenshot_save_successful = save_success;
			}
		});
	}
}
bool reshade::runtime::execute_screenshot_post_save_command(const std::filesystem::path &screenshot_path, unsigned int screenshot_count, std::string_view postfix)
{
	if (_screenshot_post_save_command.empty())
		return false;

	const std::wstring ext = _screenshot_post_save_command.extension().wstring();

	std::string command_line;
	if (ext == L".bat" || ext == L".cmd")
		command_line = "cmd /C call ";
	else if (ext == L".ps1")
		command_line = "powershell -File ";
	else if (ext == L".py")
		command_line = "python ";
	else if (ext != L".exe")
		return false;

	command_line += '\"';
	command_line += _screenshot_post_save_command.u8string();
	command_line += '\"';

	if (!_screenshot_post_save_command_arguments.empty())
	{
		command_line += ' ';
		command_line += expand_macro_string(_screenshot_post_save_command_arguments, {
			{ "AppName", g_target_executable_path.stem().u8string() },
			{ "PresetName", _current_preset_path.stem().u8string() },
			{ "BeforeAfter", std::string(postfix) },
			{ "TargetPath", screenshot_path.u8string() },
			{ "TargetDir", screenshot_path.parent_path().u8string() },
			{ "TargetFileName", screenshot_path.filename().u8string() },
			{ "TargetExt", screenshot_path.extension().u8string() },
			{ "TargetName", screenshot_path.stem().u8string() },
			{ "Count", std::to_string(screenshot_count) }
		}, _current_time);
	}

	if (!utils::execute_command(command_line, g_reshade_base_path / _screenshot_post_save_command_working_directory, _screenshot_post_save_command_hide_window))
	{
		log::message(log::level::error, "Failed to execute screenshot post-save command!");
		return false;
	}

	return true;
}

bool reshade::runtime::get_texture_data(api::resource resource, api::resource_usage state, uint8_t *pixels, api::format quantization_format)
{
	assert(quantization_format != api::format::unknown);

	const api::resource_desc desc = _device->get_resource_desc(resource);
	const api::format intermediate_format = api::format_to_default_typed(desc.texture.format, 0);

	// Copy back buffer data into system memory buffer
	api::resource intermediate;
	if (!_device->create_resource(api::resource_desc(desc.texture.width, desc.texture.height, 1, 1, intermediate_format, 1, api::memory_heap::gpu_to_cpu, api::resource_usage::copy_dest), nullptr, api::resource_usage::copy_dest, &intermediate))
	{
		log::message(log::level::error, "Failed to create system memory texture for screenshot capture!");
		return false;
	}

	_device->set_resource_name(intermediate, "ReShade screenshot texture");

	api::command_list *const cmd_list = _graphics_queue->get_immediate_command_list();
	cmd_list->barrier(resource, state, api::resource_usage::copy_source);
	cmd_list->copy_texture_region(resource, 0, nullptr, intermediate, 0, nullptr);
	cmd_list->barrier(resource, api::resource_usage::copy_source, state);

	api::fence copy_sync_fence = {};
	if (!_device->create_fence(0, api::fence_flags::none, &copy_sync_fence) || !_graphics_queue->signal(copy_sync_fence, 1) || !_device->wait(copy_sync_fence, 1))
		_graphics_queue->wait_idle();
	_device->destroy_fence(copy_sync_fence);

	// Copy data from intermediate image into output buffer
	api::subresource_data mapped_data = {};
	if (_device->map_texture_region(intermediate, 0, nullptr, api::map_access::read_only, &mapped_data))
	{
		auto mapped_pixels = static_cast<const uint8_t *>(mapped_data.data);
		const uint32_t pixels_row_pitch = api::format_row_pitch(quantization_format, desc.texture.width);

		for (size_t y = 0; y < desc.texture.height; ++y, pixels += pixels_row_pitch, mapped_pixels += mapped_data.row_pitch)
		{
			if (quantization_format == intermediate_format)
			{
				std::memcpy(pixels, mapped_pixels, pixels_row_pitch);
				continue;
			}

			if (quantization_format == api::format::r8g8b8a8_unorm)
			{
				switch (intermediate_format)
				{
				case api::format::r8_unorm:
					for (size_t x = 0; x < desc.texture.width; ++x)
					{
						pixels[x * 4 + 0] = mapped_pixels[x];
						pixels[x * 4 + 1] = 0;
						pixels[x * 4 + 2] = 0;
						pixels[x * 4 + 3] = 0xFF;
					}
					continue;
				case api::format::r8g8_unorm:
					for (size_t x = 0; x < desc.texture.width; ++x)
					{
						pixels[x * 4 + 0] = mapped_pixels[x * 2 + 0];
						pixels[x * 4 + 1] = mapped_pixels[x * 2 + 1];
						pixels[x * 4 + 2] = 0;
						pixels[x * 4 + 3] = 0xFF;
					}
					continue;
				case api::format::r8g8b8x8_unorm:
					for (size_t x = 0; x < pixels_row_pitch; x += 4)
					{
						pixels[x + 0] = mapped_pixels[x + 0];
						pixels[x + 1] = mapped_pixels[x + 1];
						pixels[x + 2] = mapped_pixels[x + 2];
						pixels[x + 3] = 0xFF;
					}
					continue;
				case api::format::b8g8r8a8_unorm:
					// Format is BGRA, but output should be RGBA, so flip channels
					for (size_t x = 0; x < pixels_row_pitch; x += 4)
					{
						pixels[x + 0] = mapped_pixels[x + 2];
						pixels[x + 1] = mapped_pixels[x + 1];
						pixels[x + 2] = mapped_pixels[x + 0];
						pixels[x + 3] = mapped_pixels[x + 3];
					}
					continue;
				case api::format::b8g8r8x8_unorm:
					for (size_t x = 0; x < pixels_row_pitch; x += 4)
					{
						pixels[x + 0] = mapped_pixels[x + 2];
						pixels[x + 1] = mapped_pixels[x + 1];
						pixels[x + 2] = mapped_pixels[x + 0];
						pixels[x + 3] = 0xFF;
					}
					continue;
				case api::format::r10g10b10a2_unorm:
				case api::format::b10g10r10a2_unorm:
					for (size_t x = 0; x < pixels_row_pitch; x += 4)
					{
						const auto offset_r = intermediate_format == api::format::b10g10r10a2_unorm ? 2 : 0;
						const auto offset_g = 1;
						const auto offset_b = intermediate_format == api::format::b10g10r10a2_unorm ? 0 : 2;
						const auto offset_a = 3;

						const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_pixels + x);
						// Divide by 4 to get 10-bit range (0-1023) into 8-bit range (0-255)
						pixels[x + offset_r] = (( rgba & 0x000003FFu)        /  4) & 0xFF;
						pixels[x + offset_g] = (((rgba & 0x000FFC00u) >> 10) /  4) & 0xFF;
						pixels[x + offset_b] = (((rgba & 0x3FF00000u) >> 20) /  4) & 0xFF;
						pixels[x + offset_a] = (((rgba & 0xC0000000u) >> 30) * 85) & 0xFF;
					}
					continue;
				}
			}
			else if (quantization_format == api::format::r16g16b16_unorm)
			{
				switch (intermediate_format)
				{
				case api::format::r10g10b10a2_unorm:
				case api::format::b10g10r10a2_unorm:
					for (size_t x = 0; x < pixels_row_pitch; x += sizeof(uint16_t) * 3)
					{
						const auto offset_r = intermediate_format == api::format::b10g10r10a2_unorm ? 2 : 0;
						const auto offset_g = 1;
						const auto offset_b = intermediate_format == api::format::b10g10r10a2_unorm ? 0 : 2;

						const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_pixels + (x / (sizeof(uint16_t) * 3)) * 4);
						// Multiply by 64 to get 10-bit range (0-1023) into 16-bit range (0-65535)
						reinterpret_cast<uint16_t *>(pixels + x)[offset_r] = ( (rgba & 0x000003FFu)        * 64) & 0xFFFF;
						reinterpret_cast<uint16_t *>(pixels + x)[offset_g] = (((rgba & 0x000FFC00u) >> 10) * 64) & 0xFFFF;
						reinterpret_cast<uint16_t *>(pixels + x)[offset_b] = (((rgba & 0x3FF00000u) >> 20) * 64) & 0xFFFF;
					}
					continue;
				}
			}
			else if (quantization_format == api::format::r16g16b16_float && intermediate_format == api::format::r16g16b16a16_float)
			{
				for (size_t x = 0; x < pixels_row_pitch; x += sizeof(uint16_t) * 3)
				{
					std::memcpy(pixels + x, mapped_pixels + (x / 3) * 4, sizeof(uint16_t) * 3);
				}
				continue;
			}
			else if (quantization_format == api::format::r10g10b10a2_unorm && intermediate_format == api::format::b10g10r10a2_unorm)
			{
				// Format is BGRA, but output should be RGBA, so flip channels
				for (size_t x = 0; x < pixels_row_pitch; x += sizeof(uint32_t))
				{
					const uint32_t rgba = *reinterpret_cast<const uint32_t *>(mapped_pixels + x);
					*reinterpret_cast<uint32_t *>(pixels + x) = ((rgba & 0x000003FFu) << 20) | ((rgba & 0x3FF00000u) >> 20) | (rgba & 0xC00FFC00u);
				}
				continue;
			}

			// Unsupported quantization, return an error below
			mapped_data.data = nullptr;

			log::message(log::level::error, "Screenshots are not supported for format %u!", static_cast<uint32_t>(desc.texture.format));
			break;
		}

		_device->unmap_texture_region(intermediate, 0);
	}

	_device->destroy_resource(intermediate);

	return mapped_data.data != nullptr;
}
