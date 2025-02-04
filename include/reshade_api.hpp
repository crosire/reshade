/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "reshade_api_device.hpp"

namespace reshade { namespace api
{
	/// <summary>
	/// An opaque handle to a technique in an effect.
	/// </summary>
	/// <remarks>
	/// This handle is only valid until effects are next reloaded again (<see cref="addon_event::reshade_reloaded_effects"/>).
	/// </remarks>
	RESHADE_DEFINE_HANDLE(effect_technique);
	/// <summary>
	/// An opaque handle to a texture variable in an effect.
	/// </summary>
	/// <remarks>
	/// This handle is only valid until effects are next reloaded again (<see cref="addon_event::reshade_reloaded_effects"/>).
	/// </remarks>
	RESHADE_DEFINE_HANDLE(effect_texture_variable);
	/// <summary>
	/// An opaque handle to a uniform variable in an effect.
	/// </summary>
	/// <remarks>
	/// This handle is only valid until effects are next reloaded again (<see cref="addon_event::reshade_reloaded_effects"/>).
	/// </remarks>
	RESHADE_DEFINE_HANDLE(effect_uniform_variable);

	/// <summary>
	/// Input source for events triggered by user input.
	/// </summary>
	enum class input_source
	{
		none = 0,
		mouse = 1,
		keyboard = 2,
		gamepad = 3,
		clipboard = 4,
	};

	/// <summary>
	/// A post-processing effect runtime, used to control effects.
	/// <para>ReShade associates an independent post-processing effect runtime with most swap chains.</para>
	/// </summary>
	struct __declspec(novtable) effect_runtime : public device_object
	{
		/// <summary>
		/// Gets the handle of the window associated with this effect runtime.
		/// </summary>
		virtual void *get_hwnd() const = 0;

		/// <summary>
		/// Gets the back buffer resource at the specified <paramref name="index"/> in the swap chain associated with this effect runtime.
		/// </summary>
		/// <param name="index">Index of the back buffer. This has to be between zero and the value returned by <see cref="get_back_buffer_count"/>.</param>
		virtual resource get_back_buffer(uint32_t index) = 0;

		/// <summary>
		/// Gets the number of back buffer resources in the swap chain associated with this effect runtime.
		/// </summary>
		virtual uint32_t get_back_buffer_count() const = 0;

		/// <summary>
		/// Gets the current back buffer resource.
		/// </summary>
		resource get_current_back_buffer() { return get_back_buffer(get_current_back_buffer_index()); }
		/// <summary>
		/// Gets the index of the back buffer resource that can currently be rendered into.
		/// </summary>
		virtual uint32_t get_current_back_buffer_index() const = 0;

		/// <summary>
		/// Gets the main graphics command queue associated with this effect runtime.
		/// This may potentially be different from the presentation queue and should be used to execute graphics commands on.
		/// </summary>
		virtual command_queue *get_command_queue() = 0;

		/// <summary>
		/// Applies post-processing effects to the specified render targets and prevents the usual rendering of effects before swap chain presentation of the current frame.
		/// This can be used to force ReShade to render effects at a certain point during the frame to e.g. avoid effects being applied to user interface elements of the application.
		/// </summary>
		/// <remarks>
		/// The resource the render target views point to has to be in the <see cref="resource_usage::render_target"/> state.
		/// This call may modify current state on the command list (pipeline, render targets, descriptor tables, ...), so it may be necessary for an add-on to backup and restore state around it if the application does not bind all state again afterwards already.
		/// Calling this with <paramref name="rtv"/> set to zero will cause nothing to be rendered, but uniform variables to still be updated.
		/// </remarks>
		/// <param name="cmd_list">Command list to add effect rendering commands to.</param>
		/// <param name="rtv">Render target view to use for passes that write to the back buffer with <c>SRGBWriteEnabled</c> state set to <see langword="false"/> (this should be a render target view of the target resource, created with a non-sRGB format variant).</param>
		/// <param name="rtv_srgb">Render target view to use for passes that write to the back buffer with <c>SRGBWriteEnabled</c> state set to <see langword="true"/> (this should be a render target view of the target resource, created with a sRGB format variant).</param>
		virtual void render_effects(command_list *cmd_list, resource_view rtv, resource_view rtv_srgb) = 0;

		/// <summary>
		/// Captures a screenshot of the current back buffer resource and returns its image data.
		/// </summary>
		/// <param name="pixels">Pointer to an array of <c>width * height * bpp</c> bytes the image data is written to (where <c>bpp</c> is the number of bytes per pixel of the back buffer format).</param>
		virtual bool capture_screenshot(void *pixels) = 0;

		/// <summary>
		/// Gets the current buffer dimensions of the swap chain.
		/// </summary>
		virtual void get_screenshot_width_and_height(uint32_t *out_width, uint32_t *out_height) const = 0;

		/// <summary>
		/// Gets the current status of the specified key.
		/// </summary>
		/// <param name="keycode">The virtual key code to check.</param>
		/// <returns><see langword="true"/> if the key is currently pressed down, <see langword="false"/> otherwise.</returns>
		virtual bool is_key_down(uint32_t keycode) const = 0;
		/// <summary>
		/// Gets whether the specified key was pressed this frame.
		/// </summary>
		/// <param name="keycode">The virtual key code to check.</param>
		/// <returns><see langword="true"/> if the key was pressed this frame, <see langword="false"/> otherwise.</returns>
		virtual bool is_key_pressed(uint32_t keycode) const = 0;
		/// <summary>
		/// Gets whether the specified key was released this frame.
		/// </summary>
		/// <param name="keycode">The virtual key code to check.</param>
		/// <returns><see langword="true"/> if the key was released this frame, <see langword="false"/> otherwise.</returns>
		virtual bool is_key_released(uint32_t keycode) const = 0;
		/// <summary>
		/// Gets the current status of the specified mouse button.
		/// </summary>
		/// <param name="button">The mouse button index to check (0 = left, 1 = middle, 2 = right).</param>
		/// <returns><see langword="true"/> if the mouse button is currently pressed down, <see langword="false"/> otherwise.</returns>
		virtual bool is_mouse_button_down(uint32_t button) const = 0;
		/// <summary>
		/// Gets whether the specified mouse button was pressed this frame.
		/// </summary>
		/// <param name="button">The mouse button index to check (0 = left, 1 = middle, 2 = right).</param>
		/// <returns><see langword="true"/> if the mouse button was pressed this frame, <see langword="false"/> otherwise.</returns>
		virtual bool is_mouse_button_pressed(uint32_t button) const = 0;
		/// <summary>
		/// Gets whether the specified mouse button was released this frame.
		/// </summary>
		/// <param name="button">The mouse button index to check (0 = left, 1 = middle, 2 = right).</param>
		/// <returns><see langword="true"/> if the mouse button was released this frame, <see langword="false"/> otherwise.</returns>
		virtual bool is_mouse_button_released(uint32_t button) const = 0;

		/// <summary>
		/// Gets the current absolute position of the mouse cursor in screen coordinates.
		/// </summary>
		/// <param name="out_x">Pointer to a variable that is set to the X coordinate of the current cursor position.</param>
		/// <param name="out_y">Pointer to a variable that is set to the Y coordinate of the current cursor position.</param>
		/// <param name="out_wheel_delta">Optional pointer to a variable that is set to the mouse wheel delta since the last frame.</param>
		virtual void get_mouse_cursor_position(uint32_t *out_x, uint32_t *out_y, int16_t *out_wheel_delta = nullptr) const = 0;

		/// <summary>
		/// Enumerates all uniform variables of loaded effects and calls the specified <paramref name="callback"/> function with a handle for each one.
		/// </summary>
		/// <param name="effect_name">File name of the effect file to enumerate uniform variables from, or <see langword="nullptr"/> to enumerate those of all loaded effects.</param>
		/// <param name="callback">Function to call for every uniform variable.</param>
		/// <param name="user_data">Optional pointer passed to the callback function.</param>
		virtual void enumerate_uniform_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, effect_uniform_variable variable, void *user_data), void *user_data) = 0;
		/// <summary>
		/// Enumerates all uniform variables of loaded effects and calls the specified callback function with a handle for each one.
		/// </summary>
		/// <param name="effect_name">File name of the effect file to enumerate uniform variables from, or <see langword="nullptr"/> to enumerate those of all loaded effects.</param>
		/// <param name="lambda">Function to call for every uniform variable.</param>
		template <typename F>
		void enumerate_uniform_variables(const char *effect_name, F lambda)
		{
			enumerate_uniform_variables(effect_name, [](effect_runtime *runtime, effect_uniform_variable variable, void *user_data) { static_cast<F *>(user_data)->operator()(runtime, variable); }, &lambda);
		}

		/// <summary>
		/// Finds a specific uniform variable in the loaded effects and returns a handle to it.
		/// </summary>
		/// <param name="effect_name">File name of the effect file the variable is declared in, or <see langword="nullptr"/> to search in all loaded effects.</param>
		/// <param name="variable_name">Name of the uniform variable declaration to find.</param>
		/// <returns>Opaque handle to the uniform variable, or zero in case it was not found.</returns>
		/// <remarks>
		/// This will not find uniform variables when performance mode is enabled, since in that case uniform variables are replaced with constants during effect compilation.
		/// </remarks>
		virtual effect_uniform_variable find_uniform_variable(const char *effect_name, const char *variable_name) const = 0;

		/// <summary>
		/// Gets information about the data type of a uniform <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="out_base_type">Optional pointer to a variable that is set to the base type of the uniform variable (<see cref="format::r32_typeless"/>, <see cref="format::r32_sint"/>, <see cref="format::r32_uint"/> or <see cref="format::r32_float"/>).</param>
		/// <param name="out_rows">Optional pointer to a variable that is set to the number of vector rows of the uniform variable type.</param>
		/// <param name="out_columns">Optional pointer to a variable that is set to the number of matrix column of the uniform variable type.</param>
		/// <param name="out_array_length">Optional pointer to a variable that is set to the number of array elements of the uniform variable type.</param>
		virtual void get_uniform_variable_type(effect_uniform_variable variable, format *out_base_type, uint32_t *out_rows = nullptr, uint32_t *out_columns = nullptr, uint32_t *out_array_length = nullptr) const = 0;

		/// <summary>
		/// Gets the name of a uniform <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Pointer to a string buffer that is filled with the name of the uniform variable, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="name_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		virtual void get_uniform_variable_name(effect_uniform_variable variable, char *name, size_t *name_size) const = 0;
		template <size_t SIZE>
		void get_uniform_variable_name(effect_uniform_variable variable, char(&name)[SIZE]) const
		{
			size_t name_size = SIZE;
			get_uniform_variable_name(variable, name, &name_size);
		}

		/// <summary>
		/// Gets the value from an annotation attached to the specified uniform <paramref name="variable"/> as boolean values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of booleans that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the uniform variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_bool_from_uniform_variable(effect_uniform_variable variable, const char *name, bool *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified uniform <paramref name="variable"/> as floating-point values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of floating-points that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the uniform variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_float_from_uniform_variable(effect_uniform_variable variable, const char *name, float *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified uniform <paramref name="variable"/> as signed integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of signed integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the uniform variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_int_from_uniform_variable(effect_uniform_variable variable, const char *name, int32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified uniform <paramref name="variable"/> as unsigned integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of unsigned integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the uniform variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_uint_from_uniform_variable(effect_uniform_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from a string annotation attached to the specified uniform <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the annotation, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="value_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		/// <returns><see langword="true"/> if the annotation exists on the uniform variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_string_from_uniform_variable(effect_uniform_variable variable, const char *name, char *value, size_t *value_size) const = 0;
		template <size_t SIZE>
		bool get_annotation_string_from_uniform_variable(effect_uniform_variable variable, const char *name, char(&value)[SIZE]) const
		{
			size_t value_size = SIZE;
			return get_annotation_string_from_uniform_variable(variable, name, value, &value_size);
		}

		/// <summary>
		/// Gets the value of the specified uniform <paramref name="variable"/> as boolean values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="values">Pointer to an array of booleans that is filled with the values of this uniform variable.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when this uniform variable is an array variable.</param>
		virtual void get_uniform_value_bool(effect_uniform_variable variable, bool *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value of the specified uniform <paramref name="variable"/> as floating-point values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="values">Pointer to an array of floating-points that is filled with the values of this uniform variable.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when this uniform variable is an array variable.</param>
		virtual void get_uniform_value_float(effect_uniform_variable variable, float *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value of the specified uniform <paramref name="variable"/> as signed integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="values">Pointer to an array of signed integers that is filled with the values of this uniform variable.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when this uniform variable is an array variable.</param>
		virtual void get_uniform_value_int(effect_uniform_variable variable, int32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value of the specified uniform <paramref name="variable"/> as unsigned integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="values">Pointer to an array of unsigned integers that is filled with the values of this uniform variable.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when this uniform variable is an array variable.</param>
		virtual void get_uniform_value_uint(effect_uniform_variable variable, uint32_t *values, size_t count, size_t array_index = 0) const = 0;

		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as boolean values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="values">Pointer to an array of booleans that are used to update this uniform variable.</param>
		/// <param name="count">Number of values to write.</param>
		/// <param name="array_index">Array offset to start writing values to when this uniform variable is an array variable.</param>
		///	<remarks>
		/// Setting the uniform value will not automatically save the current preset.
		/// To make sure the current preset with the changed value is saved to disk, explicitly call <see cref="save_current_preset"/>.
		/// </remarks>
		virtual void set_uniform_value_bool(effect_uniform_variable variable, const bool *values, size_t count, size_t array_index = 0) = 0;
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as a vector of boolean values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="x">Value of the first component in the vector that is used to update this uniform variable.</param>
		/// <param name="y">Optional value of the second component in the vector that is used to update this uniform variable.</param>
		/// <param name="z">Optional value of the third component in the vector that is used to update this uniform variable.</param>
		/// <param name="w">Optional value of the fourth component in the vector that is used to update this uniform variable.</param>
		void set_uniform_value_bool(effect_uniform_variable variable, bool x, bool y = bool(0), bool z = bool(0), bool w = bool(0))
		{
			const bool values[4] = { x, y, z, w };
			set_uniform_value_bool(variable, values, 4);
		}
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as floating-point values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="values">Pointer to an array of floating-points that are used to update this uniform variable.</param>
		/// <param name="count">Number of values to write.</param>
		/// <param name="array_index">Array offset to start writing values to when this uniform variable is an array variable.</param>
		///	<remarks>
		/// Setting the uniform value will not automatically save the current preset.
		/// To make sure the current preset with the changed value is saved to disk, explicitly call <see cref="save_current_preset"/>.
		/// </remarks>
		virtual void set_uniform_value_float(effect_uniform_variable variable, const float *values, size_t count, size_t array_index = 0) = 0;
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as a vector of floating-point values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="x">Value of the first component in the vector that is used to update this uniform variable.</param>
		/// <param name="y">Optional value of the second component in the vector that is used to update this uniform variable.</param>
		/// <param name="z">Optional value of the third component in the vector that is used to update this uniform variable.</param>
		/// <param name="w">Optional value of the fourth component in the vector that is used to update this uniform variable.</param>
		void set_uniform_value_float(effect_uniform_variable variable, float x, float y = float(0), float z = float(0), float w = float(0))
		{
			const float values[4] = { x, y, z, w };
			set_uniform_value_float(variable, values, 4);
		}
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as signed integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="values">Pointer to an array of signed integers that are used to update this uniform variable.</param>
		/// <param name="count">Number of values to write.</param>
		/// <param name="array_index">Array offset to start writing values to when this uniform variable is an array variable.</param>
		///	<remarks>
		/// Setting the uniform value will not automatically save the current preset.
		/// To make sure the current preset with the changed value is saved to disk, explicitly call <see cref="save_current_preset"/>.
		/// </remarks>
		virtual void set_uniform_value_int(effect_uniform_variable variable, const int32_t *values, size_t count, size_t array_index = 0) = 0;
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as a vector of signed integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="x">Value of the first component in the vector that is used to update this uniform variable.</param>
		/// <param name="y">Optional value of the second component in the vector that is used to update this uniform variable.</param>
		/// <param name="z">Optional value of the third component in the vector that is used to update this uniform variable.</param>
		/// <param name="w">Optional value of the fourth component in the vector that is used to update this uniform variable.</param>
		void set_uniform_value_int(effect_uniform_variable variable, int32_t x, int32_t y = int32_t(0), int32_t z = int32_t(0), int32_t w = int32_t(0))
		{
			const int32_t values[4] = { x, y, z, w };
			set_uniform_value_int(variable, values, 4);
		}
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as unsigned integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="values">Pointer to an array of unsigned integers that are used to update this uniform variable.</param>
		/// <param name="count">Number of values to write.</param>
		/// <param name="array_index">Array offset to start writing values to when this uniform variable is an array variable.</param>
		///	<remarks>
		/// Setting the uniform value will not automatically save the current preset.
		/// To make sure the current preset with the changed value is saved to disk, explicitly call <see cref="save_current_preset"/>.
		/// </remarks>
		virtual void set_uniform_value_uint(effect_uniform_variable variable, const uint32_t *values, size_t count, size_t array_index = 0) = 0;
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as a vector of unsigned integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="x">Value of the first component in the vector that is used to update this uniform variable.</param>
		/// <param name="y">Optional value of the second component in the vector that is used to update this uniform variable.</param>
		/// <param name="z">Optional value of the third component in the vector that is used to update this uniform variable.</param>
		/// <param name="w">Optional value of the fourth component in the vector that is used to update this uniform variable.</param>
		void set_uniform_value_uint(effect_uniform_variable variable, uint32_t x, uint32_t y = uint32_t(0), uint32_t z = uint32_t(0), uint32_t w = uint32_t(0))
		{
			const uint32_t values[4] = { x, y, z, w };
			set_uniform_value_uint(variable, values, 4);
		}

		/// <summary>
		/// Enumerates all texture variables of loaded effects and calls the specified <paramref name="callback"/> function with a handle for each one.
		/// </summary>
		/// <param name="effect_name">File name of the effect file to enumerate texture variables from, or <see langword="nullptr"/> to enumerate those of all loaded effects.</param>
		/// <param name="callback">Function to call for every texture variable.</param>
		/// <param name="user_data">Optional pointer passed to the callback function.</param>
		virtual void enumerate_texture_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, effect_texture_variable variable, void *user_data), void *user_data) = 0;
		/// <summary>
		/// Enumerates all texture variables of loaded effects and calls the specified callback function with a handle for each one.
		/// </summary>
		/// <param name="effect_name">File name of the effect file to enumerate texture variables from, or <see langword="nullptr"/> to enumerate those of all loaded effects.</param>
		/// <param name="lambda">Function to call for every texture variable.</param>
		template <typename F>
		void enumerate_texture_variables(const char *effect_name, F lambda)
		{
			enumerate_texture_variables(effect_name, [](effect_runtime *runtime, effect_texture_variable variable, void *user_data) { static_cast<F *>(user_data)->operator()(runtime, variable); }, &lambda);
		}

		/// <summary>
		/// Finds a specific texture variable in the loaded effects and returns a handle to it.
		/// </summary>
		/// <param name="effect_name">File name of the effect file the variable is declared in, or <see langword="nullptr"/> to search in all loaded effects.</param>
		/// <param name="variable_name">Name of the texture variable declaration to find.</param>
		/// <returns>Opaque handle to the texture variable, or zero in case it was not found.</returns>
		virtual effect_texture_variable find_texture_variable(const char *effect_name, const char *variable_name) const = 0;

		/// <summary>
		/// Gets the name of a texture <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Pointer to a string buffer that is filled with the name of the texture variable, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="name_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		virtual void get_texture_variable_name(effect_texture_variable variable, char *name, size_t *name_size) const = 0;
		template <size_t SIZE>
		void get_texture_variable_name(effect_texture_variable variable, char(&name)[SIZE]) const
		{
			size_t name_size = SIZE;
			get_texture_variable_name(variable, name, &name_size);
		}

		/// <summary>
		/// Gets the value from an annotation attached to the specified texture <paramref name="variable"/> as boolean values.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of booleans that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the texture variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_bool_from_texture_variable(effect_texture_variable variable, const char *name, bool *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified texture <paramref name="variable"/> as floating-point values.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of floating-points that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the texture variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_float_from_texture_variable(effect_texture_variable variable, const char *name, float *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified texture <paramref name="variable"/> as signed integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of signed integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the texture variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_int_from_texture_variable(effect_texture_variable variable, const char *name, int32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified texture <paramref name="variable"/> as unsigned integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of unsigned integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the texture variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_uint_from_texture_variable(effect_texture_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from a string annotation attached to the specified texture <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the annotation, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="value_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		/// <returns><see langword="true"/> if the annotation exists on the texture variable, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_string_from_texture_variable(effect_texture_variable variable, const char *name, char *value, size_t *value_size) const = 0;
		template <size_t SIZE>
		bool get_annotation_string_from_texture_variable(effect_texture_variable variable, const char *name, char(&value)[SIZE]) const
		{
			size_t value_size = SIZE;
			return get_annotation_string_from_texture_variable(variable, name, value, &value_size);
		}

		/// <summary>
		/// Uploads image data to the specified texture <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="width">Width of the image data.</param>
		/// <param name="height">Height of the image data.</param>
		/// <param name="pixels">Pointer to an array of <c>width * height * bpp</c> bytes the image data is read from (where <c>bpp</c> is the number of bytes per pixel of the texture format).</param>
		virtual void update_texture(effect_texture_variable variable, const uint32_t width, const uint32_t height, const void *pixels) = 0;

		/// <summary>
		/// Gets the shader resource view that is bound to the specified texture <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="out_srv">Pointer to a variable that is set to the shader resource view.</param>
		/// <param name="out_srv_srgb">Pointer to a variable that is set to the sRGB shader resource view.</param>
		virtual void get_texture_binding(effect_texture_variable variable, resource_view *out_srv, resource_view *out_srv_srgb) const = 0;

		/// <summary>
		/// Binds new shader resource views to all texture variables that use the specified <paramref name="semantic"/>.
		/// </summary>
		/// <remarks>
		/// The resource the shader resource views point to has to be in the <see cref="resource_usage::shader_resource"/> state at the time <see cref="render_effects"/> or <see cref="render_technique"/> is executed.
		/// </remarks>
		/// <param name="semantic">ReShade FX semantic to filter textures to update by (<c>texture name : SEMANTIC</c>).</param>
		/// <param name="srv">Shader resource view to use for samplers with <c>SRGBTexture</c> state set to <see langword="false"/> (this should be a shader resource view of the target resource, created with a non-sRGB format variant).</param>
		/// <param name="srv_srgb">Shader resource view to use for samplers with <c>SRGBTexture</c> state set to <see langword="true"/> (this should be a shader resource view of the target resource, created with a sRGB format variant).</param>
		virtual void update_texture_bindings(const char *semantic, resource_view srv, resource_view srv_srgb) = 0;

		/// <summary>
		/// Enumerates all techniques of loaded effects and calls the specified <paramref name="callback"/> function with a handle for each one.
		/// </summary>
		/// <param name="effect_name">File name of the effect file to enumerate techniques from, or <see langword="nullptr"/> to enumerate those of all loaded effects.</param>
		/// <param name="callback">Function to call for every technique.</param>
		/// <param name="user_data">Optional pointer passed to the callback function.</param>
		virtual void enumerate_techniques(const char *effect_name, void(*callback)(effect_runtime *runtime, effect_technique technique, void *user_data), void *user_data) = 0;
		/// <summary>
		/// Enumerates all techniques of loaded effects and calls the specified callback function with a handle for each one.
		/// </summary>
		/// <param name="effect_name">File name of the effect file to enumerate techniques from, or <see langword="nullptr"/> to enumerate those of all loaded effects.</param>
		/// <param name="lambda">Function to call for every technique.</param>
		template <typename F>
		void enumerate_techniques(const char *effect_name, F lambda)
		{
			enumerate_techniques(effect_name, [](effect_runtime *runtime, effect_technique technique, void *user_data) { static_cast<F *>(user_data)->operator()(runtime, technique); }, &lambda);
		}

		/// <summary>
		/// Finds a specific technique in the loaded effects and returns a handle to it.
		/// </summary>
		/// <param name="effect_name">File name of the effect file the technique is declared in, or <see langword="nullptr"/> to search in all loaded effects.</param>
		/// <param name="technique_name">Name of the technique to find.</param>
		/// <returns>Opaque handle to the technique, or zero in case it was not found.</returns>
		virtual effect_technique find_technique(const char *effect_name, const char *technique_name) = 0;

		/// <summary>
		/// Gets the name of a <paramref name="technique"/>.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Pointer to a string buffer that is filled with the name of the technique, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="name_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		virtual void get_technique_name(effect_technique technique, char *name, size_t *name_size) const = 0;
		template <size_t SIZE>
		void get_technique_name(effect_technique technique, char(&name)[SIZE]) const
		{
			size_t name_size = SIZE;
			get_technique_name(technique, name, &name_size);
		}

		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/> as boolean values.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of booleans that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the technique, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_bool_from_technique(effect_technique technique, const char *name, bool *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/> as floating-point values.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of floating-points that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the technique, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_float_from_technique(effect_technique technique, const char *name, float *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/> as signed integer values.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of signed integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the technique, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_int_from_technique(effect_technique technique, const char *name, int32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/> as unsigned integer values.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of unsigned integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		/// <returns><see langword="true"/> if the annotation exists on the technique, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_uint_from_technique(effect_technique technique, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from a string annotation attached to the specified <paramref name="technique"/>.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the annotation, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="value_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		/// <returns><see langword="true"/> if the annotation exists on the technique, <see langword="false"/> otherwise.</returns>
		virtual bool get_annotation_string_from_technique(effect_technique technique, const char *name, char *value, size_t *value_size) const = 0;
		template <size_t SIZE>
		bool get_annotation_string_from_technique(effect_technique technique, const char *name, char(&value)[SIZE]) const
		{
			size_t value_size = SIZE;
			return get_annotation_string_from_technique(technique, name, value, &value_size);
		}

		/// <summary>
		/// Gets the state of a <paramref name="technique"/>.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <returns><see langword="true"/> if the technique is enabled, or <see langword="false"/> if it is disabled.</returns>
		virtual bool get_technique_state(effect_technique technique) const = 0;
		/// <summary>
		/// Enables or disables the specified <paramref name="technique"/>.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="enabled">Set to <see langword="true"/> to enable the technique, or <see langword="false"/> to disable it.</param>
		virtual void set_technique_state(effect_technique technique, bool enabled) = 0;

		/// <summary>
		/// Gets the value of a preprocessor definition.
		/// </summary>
		/// <param name="name">Name of the definition.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the definition, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="value_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		/// <returns><see langword="true"/> if the preprocessor definition is defined, <see langword="false"/> otherwise.</returns>
		virtual bool get_preprocessor_definition(const char *name, char *value, size_t *value_size) const = 0;
		template <size_t SIZE>
		bool get_preprocessor_definition(const char *name, char(&value)[SIZE]) const
		{
			size_t value_size = SIZE;
			return get_preprocessor_definition(name, value, &value_size);
		}
		/// <summary>
		/// Defines a preprocessor definition to the specified <paramref name="value"/>.
		/// </summary>
		/// <param name="name">Name of the definition.</param>
		/// <param name="value">Value of the definition.</param>
		virtual void set_preprocessor_definition(const char *name, const char *value) = 0;

		/// <summary>
		/// Applies a <paramref name="technique"/> to the specified render targets (regardless of the state of this technique).
		/// </summary>
		/// <remarks>
		/// The width and height of the specified render target should match those used to render all other effects!
		/// The resource the render target views point to has to be in the <see cref="resource_usage::render_target"/> state.
		/// This call may modify current state on the command list (pipeline, render targets, descriptor tables, ...), so it may be necessary for an add-on to backup and restore state around it if the application does not bind all state again afterwards already.
		/// </remarks>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="cmd_list">Command list to add effect rendering commands to.</param>
		/// <param name="rtv">Render target view to use for passes that write to the back buffer with <c>SRGBWriteEnabled</c> state set to <see langword="false"/>.</param>
		/// <param name="rtv_srgb">Render target view to use for passes that write to the back buffer with <c>SRGBWriteEnabled</c> state set to <see langword="true"/>, or zero in which case the view from <paramref name="rtv"/> is used.</param>
		virtual void render_technique(effect_technique technique, command_list *cmd_list, resource_view rtv, resource_view rtv_srgb = { 0 }) = 0;

		/// <summary>
		/// Gets whether rendering of effects is enabled or disabled.
		/// </summary>
		virtual bool get_effects_state() const = 0;
		/// <summary>
		/// Enables or disables all effects.
		/// </summary>
		/// <param name="enabled">Set to <see langword="true"/> to enable effects, or <see langword="false"/> to disable them.</param>
		virtual void set_effects_state(bool enabled) = 0;

		/// <summary>
		/// Gets the file path to the currently active preset.
		/// </summary>
		/// <param name="path">Pointer to a string buffer that is filled with the file path to the preset, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="path_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		virtual void get_current_preset_path(char *path, size_t *path_size) const = 0;
		template <size_t SIZE>
		void get_current_preset_path(char(&path)[SIZE]) const
		{
			size_t path_size = SIZE;
			get_current_preset_path(path, &path_size);
		}
		/// <summary>
		/// Saves the currently active preset and then switches to the specified new preset.
		/// </summary>
		/// <param name="path">File path to the preset to switch to.</param>
		virtual void set_current_preset_path(const char *path) = 0;

		/// <summary>
		/// Changes the rendering order of loaded techniques to that of the specified technique list.
		/// </summary>
		/// <param name="count">Number of handles in the technique list.</param>
		/// <param name="techniques">Array of techniques in the order they should be rendered in.</param>
		virtual void reorder_techniques(size_t count, const effect_technique *techniques) = 0;

		/// <summary>
		/// Makes ReShade block any keyboard and mouse input from reaching the game for the duration of the next frame.
		/// Call this every frame for as long as input should be blocked. This can be used to ensure input is only applied to overlays created in a <see cref="addon_event::reshade_overlay"/> callback.
		/// </summary>
		virtual void block_input_next_frame() = 0;

		/// <summary>
		/// Gets the virtual key code of the last key that was pressed.
		/// </summary>
		virtual uint32_t last_key_pressed() const = 0;
		/// <summary>
		/// Gets the virtual key code of the last key that was released.
		/// </summary>
		virtual uint32_t last_key_released() const = 0;

		/// <summary>
		/// Gets the effect file name of a uniform <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="effect_name">Pointer to a string buffer that is filled with the effect file name of the uniform variable, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="effect_name_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		virtual void get_uniform_variable_effect_name(effect_uniform_variable variable, char *effect_name, size_t *effect_name_size) const = 0;
		template <size_t SIZE>
		void get_uniform_variable_effect_name(effect_uniform_variable variable, char(&effect_name)[SIZE]) const
		{
			size_t effect_name_size = SIZE;
			get_uniform_variable_effect_name(variable, effect_name, &effect_name_size);
		}

		/// <summary>
		/// Gets the effect file name of a texture <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="effect_name">Pointer to a string buffer that is filled with the effect file name of the texture variable, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="effect_name_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		virtual void get_texture_variable_effect_name(effect_texture_variable variable, char *effect_name, size_t *effect_name_size) const = 0;
		template <size_t SIZE>
		void get_texture_variable_effect_name(effect_texture_variable variable, char(&effect_name)[SIZE]) const
		{
			size_t effect_name_size = SIZE;
			get_texture_variable_effect_name(variable, effect_name, &effect_name_size);
		}

		/// <summary>
		/// Gets the effect file name of a <paramref name="technique"/>.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="effect_name">Pointer to a string buffer that is filled with the effect file name of the technique, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="effect_name_size">Pointer to an integer that contains the size of the string buffer and is set to the actual length of the string, including the null-terminator.</param>
		virtual void get_technique_effect_name(effect_technique technique, char *effect_name, size_t *effect_name_size) const = 0;
		template <size_t SIZE>
		void get_technique_effect_name(effect_technique technique, char(&effect_name)[SIZE]) const
		{
			size_t effect_name_size = SIZE;
			get_technique_effect_name(technique, effect_name, &effect_name_size);
		}

		/// <summary>
		/// Saves the current preset with the current state of the loaded techniques and uniform variables.
		/// </summary>
		virtual void save_current_preset() const = 0;

		/// <summary>
		/// Gets the value of a preprocessor definition for the specified effect.
		/// </summary>
		/// <param name="effect_name">File name of the effect file the preprocessor definition is defined for.</param>
		/// <param name="name">Name of the definition.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the definition, or <see langword="nullptr"/> to query the necessary size.</param>
		/// <param name="value_size">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string, including the null-terminator.</param>
		/// <returns><see langword="true"/> if the preprocessor definition is defined, <see langword="false"/> otherwise.</returns>
		virtual bool get_preprocessor_definition_for_effect(const char *effect_name, const char *name, char *value, size_t *value_size) const = 0;
		template <size_t SIZE>
		bool get_preprocessor_definition_for_effect(const char *effect_name, const char *name, char(&value)[SIZE]) const
		{
			size_t value_size = SIZE;
			return get_preprocessor_definition_for_effect(effect_name, name, value, &value_size);
		}
		/// <summary>
		/// Defines a preprocessor definition for the specified effect to the specified <paramref name="value"/>.
		/// </summary>
		/// <param name="effect_name">File name of the effect file the preprocessor definition should be defined for.</param>
		/// <param name="name">Name of the definition.</param>
		/// <param name="value">Value of the definition.</param>
		virtual void set_preprocessor_definition_for_effect(const char *effect_name, const char *name, const char *value) = 0;

		/// <summary>
		/// Open or close the ReShade overlay.
		/// </summary>
		/// <param name="open">Requested overlay state.</param>
		/// <param name="source">Source of this request.</param>
		/// <returns><see langword="true"/> if the overlay state was changed, <see langword="false"/> otherwise.</returns>
		virtual bool open_overlay(bool open, input_source source) = 0;

		/// <summary>
		/// Overrides the color space used for presentation.
		/// </summary>
		virtual void set_color_space(color_space color_space) = 0;

		/// <summary>
		/// Resets the value of the specified uniform <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		virtual void reset_uniform_value(effect_uniform_variable variable) = 0;

		/// <summary>
		/// Queues up the specified effect for reloading in the next frame.
		/// This can be called multiple times with different effects to append to the queue.
		/// </summary>
		/// <param name="effect_name">File name of the effect file that should be reloaded, or <see langword="nullptr"/> to reload all effects.</param>
		virtual void reload_effect_next_frame(const char *effect_name) = 0;

		/// <summary>
		/// Export the current preset with the current state of the loaded techniques and uniform variables.
		/// </summary>
		/// <param name="path">File path to the preset to save to.</param>
		virtual void export_current_preset(const char *path) const = 0;
	};
} }
