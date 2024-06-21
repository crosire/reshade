/*
 * Copyright (C) 2014 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

template <typename T, size_t SAMPLES>
class moving_average
{
public:
	moving_average() : _index(0), _average(0), _tick_sum(0), _tick_list() {}

	operator T() const { return _average; }

	void clear()
	{
		_index = 0;
		_average = 0;
		_tick_sum = 0;

		for (size_t i = 0; i < SAMPLES; i++)
			_tick_list[i] = 0;
	}
	void append(T value)
	{
		_tick_sum -= _tick_list[_index];
		_tick_sum += _tick_list[_index] = value;
		_index = ++_index % SAMPLES;
		_average = _tick_sum / SAMPLES;
	}

private:
	size_t _index;
	T _average, _tick_sum, _tick_list[SAMPLES];
};
