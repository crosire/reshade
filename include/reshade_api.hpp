/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include "reshade_api_device.hpp"

namespace reshade::api
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
	/// A ReShade effect runtime, used to control effects.
	/// <para>A separate runtime is instantiated for every swap chain.</para>
	/// </summary>
	RESHADE_DEFINE_INTERFACE_WITH_BASE(effect_runtime, swapchain)
	{
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
		/// The width and height of the specified render target have to match those reported by <see cref="effect_runtime::get_screenshot_width_and_height"/>!
		/// The resource the render target views point to has to be in the <see cref="resource_usage::render_target"/> state.
		/// </remarks>
		/// <param name="cmd_list">Command list to add effect rendering commands to.</param>
		/// <param name="rtv">Render target view to use for passes that write to the back buffer with <c>SRGBWriteEnabled</c> state set to <see langword="false"/>.</param>
		/// <param name="rtv_srgb">Render target view to use for passes that write to the back buffer with <c>SRGBWriteEnabled</c> state set to <see langword="true"/>, or zero in which case the view from <paramref name="rtv"/> is used.</param>
		virtual void render_effects(command_list *cmd_list, resource_view rtv, resource_view rtv_srgb = { 0 }) = 0;

		/// <summary>
		/// Captures a screenshot of the current back buffer resource and returns its image data in 32 bits-per-pixel RGBA format.
		/// </summary>
		/// <param name="pixels">Pointer to an array of <c>width * height * 4</c> bytes the image data is written to.</param>
		virtual bool capture_screenshot(uint8_t *pixels) = 0;

		/// <summary>
		/// Gets the current buffer dimensions of the swap chain as used with effect rendering.
		/// The returned values are equivalent to <c>BUFFER_WIDTH</c> and <c>BUFFER_HEIGHT</c> in ReShade FX.
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
		inline  void enumerate_uniform_variables(const char *effect_name, F lambda) {
			enumerate_uniform_variables(effect_name, [](effect_runtime *runtime, effect_uniform_variable variable, void *user_data) { static_cast<F *>(user_data)->operator()(runtime, variable); }, &lambda);
		}

		/// <summary>
		/// Finds a specific uniform variable in the loaded effects and returns a handle to it.
		/// </summary>
		/// <param name="effect_name">File name of the effect file the variable is declared in, or <see langword="nullptr"/> to search in all loaded effects.</param>
		/// <param name="variable_name">Name of the uniform variable declaration to find.</param>
		/// <returns>Opaque handle to the uniform variable, or zero in case it was not found.</returns>
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
		/// <param name="name">Pointer to a string buffer that is filled with the name of the uniform variable.</param>
		/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
		virtual void get_uniform_variable_name(effect_uniform_variable variable, char *name, size_t *length) const = 0;
		template <size_t SIZE>
		inline  void get_uniform_variable_name(effect_uniform_variable variable, char(&name)[SIZE]) const {
			size_t length = SIZE;
			get_uniform_variable_name(variable, name, &length);
		}

		/// <summary>
		/// Gets the value from an annotation attached to the specified uniform <paramref name="variable"/> as boolean values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of booleans that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_bool_from_uniform_variable(effect_uniform_variable variable, const char *name, bool *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified uniform <paramref name="variable"/> as floating-point values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of floating-points that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_float_from_uniform_variable(effect_uniform_variable variable, const char *name, float *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified uniform <paramref name="variable"/> as signed integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of signed integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_int_from_uniform_variable(effect_uniform_variable variable, const char *name, int32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified uniform <paramref name="variable"/> as unsigned integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of unsigned integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_uint_from_uniform_variable(effect_uniform_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from a string annotation attached to the specified uniform <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the annotation.</param>
		/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
		virtual bool get_annotation_string_from_uniform_variable(effect_uniform_variable variable, const char *name, char *value, size_t *length) const = 0;
		template <size_t SIZE>
		inline  bool get_annotation_string_from_uniform_variable(effect_uniform_variable variable, const char *name, char(&value)[SIZE]) const {
			size_t length = SIZE;
			return get_annotation_string_from_uniform_variable(variable, name, value, &length);
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
		virtual void set_uniform_value_bool(effect_uniform_variable variable, const bool *values, size_t count, size_t array_index = 0) = 0;
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as a vector of boolean values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="x">Value of the first component in the vector that is used to update this uniform variable.</param>
		/// <param name="y">Optional value of the second component in the vector that is used to update this uniform variable.</param>
		/// <param name="z">Optional value of the third component in the vector that is used to update this uniform variable.</param>
		/// <param name="w">Optional value of the fourth component in the vector that is used to update this uniform variable.</param>
		inline  void set_uniform_value_bool(effect_uniform_variable variable, bool x, bool y = bool(0), bool z = bool(0), bool w = bool(0)) {
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
		virtual void set_uniform_value_float(effect_uniform_variable variable, const float *values, size_t count, size_t array_index = 0) = 0;
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as a vector of floating-point values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="x">Value of the first component in the vector that is used to update this uniform variable.</param>
		/// <param name="y">Optional value of the second component in the vector that is used to update this uniform variable.</param>
		/// <param name="z">Optional value of the third component in the vector that is used to update this uniform variable.</param>
		/// <param name="w">Optional value of the fourth component in the vector that is used to update this uniform variable.</param>
		inline  void set_uniform_value_float(effect_uniform_variable variable, float x, float y = float(0), float z = float(0), float w = float(0)) {
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
		virtual void set_uniform_value_int(effect_uniform_variable variable, const int32_t *values, size_t count, size_t array_index = 0) = 0;
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as a vector of signed integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="x">Value of the first component in the vector that is used to update this uniform variable.</param>
		/// <param name="y">Optional value of the second component in the vector that is used to update this uniform variable.</param>
		/// <param name="z">Optional value of the third component in the vector that is used to update this uniform variable.</param>
		/// <param name="w">Optional value of the fourth component in the vector that is used to update this uniform variable.</param>
		inline  void set_uniform_value_int(effect_uniform_variable variable, int32_t x, int32_t y = int32_t(0), int32_t z = int32_t(0), int32_t w = int32_t(0)) {
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
		virtual void set_uniform_value_uint(effect_uniform_variable variable, const uint32_t *values, size_t count, size_t array_index = 0) = 0;
		/// <summary>
		/// Sets the value of the specified uniform <paramref name="variable"/> as a vector of unsigned integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the uniform variable.</param>
		/// <param name="x">Value of the first component in the vector that is used to update this uniform variable.</param>
		/// <param name="y">Optional value of the second component in the vector that is used to update this uniform variable.</param>
		/// <param name="z">Optional value of the third component in the vector that is used to update this uniform variable.</param>
		/// <param name="w">Optional value of the fourth component in the vector that is used to update this uniform variable.</param>
		inline  void set_uniform_value_uint(effect_uniform_variable variable, uint32_t x, uint32_t y = uint32_t(0), uint32_t z = uint32_t(0), uint32_t w = uint32_t(0)) {
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
		inline  void enumerate_texture_variables(const char *effect_name, F lambda) {
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
		/// <param name="name">Pointer to a string buffer that is filled with the name of the texture variable.</param>
		/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
		virtual void get_texture_variable_name(effect_texture_variable variable, char *name, size_t *length) const = 0;
		template <size_t SIZE>
		inline  void get_texture_variable_name(effect_texture_variable variable, char(&name)[SIZE]) const {
			size_t length = SIZE;
			get_texture_variable_name(variable, name, &length);
		}

		/// <summary>
		/// Gets the value from an annotation attached to the specified texture <paramref name="variable"/> as boolean values.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of booleans that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_bool_from_texture_variable(effect_texture_variable variable, const char *name, bool *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified texture <paramref name="variable"/> as floating-point values.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of floating-points that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_float_from_texture_variable(effect_texture_variable variable, const char *name, float *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified texture <paramref name="variable"/> as signed integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of signed integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_int_from_texture_variable(effect_texture_variable variable, const char *name, int32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified texture <paramref name="variable"/> as unsigned integer values.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of unsigned integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_uint_from_texture_variable(effect_texture_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from a string annotation attached to the specified texture <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the annotation.</param>
		/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
		virtual bool get_annotation_string_from_texture_variable(effect_texture_variable variable, const char *name, char *value, size_t *length) const = 0;
		template <size_t SIZE>
		inline  bool get_annotation_string_from_texture_variable(effect_texture_variable variable, const char *name, char(&value)[SIZE]) const {
			size_t length = SIZE;
			return get_annotation_string_from_texture_variable(variable, name, value, &length);
		}

		/// <summary>
		/// Uploads 32 bits-per-pixel RGBA image data to the specified texture <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="width">Width of the image data.</param>
		/// <param name="height">Height of the image data.</param>
		/// <param name="pixels">Pointer to an array of <c>width * height * 4</c> bytes the image data is read from.</param>
		virtual void update_texture(effect_texture_variable variable, const uint32_t width, const uint32_t height, const uint8_t *pixels) = 0;

		/// <summary>
		/// Gets the shader resource view that is bound to the specified texture <paramref name="variable"/>.
		/// </summary>
		/// <param name="variable">Opaque handle to the texture variable.</param>
		/// <param name="out_srv">Pointer to a variable that is set to the shader resource view.</param>
		/// <param name="out_srv_srgb">Pointer to a variable that is set to the sRGB shader resource view.</param>
		virtual void get_texture_binding(effect_texture_variable variable, resource_view *out_srv, resource_view *out_srv_srgb = nullptr) const = 0;

		/// <summary>
		/// Binds a new shader resource view to all texture variables that use the specified <paramref name="semantic"/>.
		/// </summary>
		/// <remarks>
		/// The resource the shader resource views point to has to be in the <see cref="resource_usage::shader_resource"/> state at the time <see cref="render_effects"/> is executed.
		/// </remarks>
		/// <param name="semantic">ReShade FX semantic to filter textures to update by (<c>texture name : SEMANTIC</c>).</param>
		/// <param name="srv">Shader resource view to use for samplers with <c>SRGBTexture</c> state set to <see langword="false"/>.</param>
		/// <param name="srv_srgb">Shader resource view to use for samplers with <c>SRGBTexture</c> state set to <see langword="true"/>, or zero in which case the view from <paramref name="srv"/> is used.</param>
		virtual void update_texture_bindings(const char *semantic, resource_view srv, resource_view srv_srgb = { 0 }) = 0;

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
		inline  void enumerate_techniques(const char *effect_name, F lambda) {
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
		/// <param name="name">Pointer to a string buffer that is filled with the name of the technique.</param>
		/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
		virtual void get_technique_name(effect_technique technique, char *name, size_t *length) const = 0;
		template <size_t SIZE>
		inline  void get_technique_name(effect_technique technique, char(&name)[SIZE]) const {
			size_t length = SIZE;
			get_technique_name(technique, name, &length);
		}

		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/> as boolean values.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of booleans that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_bool_from_technique(effect_technique technique, const char *name, bool *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/> as floating-point values.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of floating-points that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_float_from_technique(effect_technique technique, const char *name, float *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/> as signed integer values.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of signed integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_int_from_technique(effect_technique technique, const char *name, int32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from an annotation attached to the specified <paramref name="technique"/> as unsigned integer values.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="values">Pointer to an array of unsigned integers that is filled with the values of the annotation.</param>
		/// <param name="count">Number of values to read.</param>
		/// <param name="array_index">Array offset to start reading values from when the annotation is an array.</param>
		virtual bool get_annotation_uint_from_technique(effect_technique technique, const char *name, uint32_t *values, size_t count, size_t array_index = 0) const = 0;
		/// <summary>
		/// Gets the value from a string annotation attached to the specified <paramref name="technique"/>.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="name">Name of the annotation.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the annotation.</param>
		/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
		virtual bool get_annotation_string_from_technique(effect_technique technique, const char *name, char *value, size_t *length) const = 0;
		template <size_t SIZE>
		inline  bool get_annotation_string_from_technique(effect_technique technique, const char *name, char(&value)[SIZE]) const {
			size_t length = SIZE;
			return get_annotation_string_from_technique(technique, name, value, &length);
		}

		/// <summary>
		/// Gets the state of a <paramref name="technique"/>.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		virtual bool get_technique_state(effect_technique technique) const = 0;
		/// <summary>
		/// Enables or disables the specified <paramref name="technique"/>.
		/// </summary>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="enabled">Set to <see langword="true"/> to enable the technique, or <see langword="false"/> to disable it.</param>
		virtual void set_technique_state(effect_technique technique, bool enabled) = 0;

		/// <summary>
		/// Gets the value of global preprocessor definition.
		/// </summary>
		/// <param name="name">Name of the definition.</param>
		/// <param name="value">Pointer to a string buffer that is filled with the value of the definition.</param>
		/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
		virtual bool get_preprocessor_definition(const char *name, char *value, size_t *length) const = 0;
		template <size_t SIZE>
		inline  bool get_preprocessor_definition(const char *name, char(&value)[SIZE]) const {
			size_t length = SIZE;
			return get_preprocessor_definition(name, value, &length);
		}
		/// <summary>
		/// Defines a global preprocessor definition to the specified <paramref name="value"/>.
		/// </summary>
		/// <param name="name">Name of the definition.</param>
		/// <param name="value">Value of the definition.</param>
		virtual void set_preprocessor_definition(const char *name, const char *value) = 0;

		/// <summary>
		/// Applies a <paramref name="technique"/> to the specified render targets (regardless of the state of this technique).
		/// </summary>
		/// <remarks>
		/// The width and height of the specified render target have to match those reported by <see cref="effect_runtime::get_screenshot_width_and_height"/>!
		/// The resource the render target views point to has to be in the <see cref="resource_usage::render_target"/> state.
		/// </remarks>
		/// <param name="technique">Opaque handle to the technique.</param>
		/// <param name="cmd_list">Command list to add effect rendering commands to.</param>
		/// <param name="rtv">Render target view to use for passes that write to the back buffer with <c>SRGBWriteEnabled</c> state set to <see langword="false"/>.</param>
		/// <param name="rtv_srgb">Render target view to use for passes that write to the back buffer with <c>SRGBWriteEnabled</c> state set to <see langword="true"/>, or zero in which case the view from <paramref name="rtv"/> is used.</param>
		virtual void render_technique(effect_technique technique, command_list *cmd_list, resource_view rtv, resource_view rtv_srgb = { 0 }) = 0;

		/// <summary>
		/// Gets whether effects are enabled or disabled.
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
		/// <param name="name">Pointer to a string buffer that is filled with the file path to the preset.</param>
		/// <param name="length">Pointer to an integer that contains the size of the string buffer and upon completion is set to the actual length of the string.</param>
		virtual void get_current_preset_path(char *path, size_t *length) const = 0;
		template <size_t SIZE>
		inline  void get_current_preset_path(char(&path)[SIZE]) const {
			size_t length = SIZE;
			get_current_preset_path(path, &length);
		}
		/// <summary>
		/// Saves the currently active preset and then switches to the specified new preset.
		/// </summary>
		/// <param name="path">File path to the preset to switch to.</param>
		virtual void set_current_preset_path(const char *path) = 0;
	};
}
