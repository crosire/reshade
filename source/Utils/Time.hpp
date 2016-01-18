#pragma once

namespace reshade
{
	namespace utils
	{
		class framerate
		{
		public:
			framerate() : _index(0), _tick_sum(0), _tick_list()
			{
			}

			operator float() const
			{
				return _framerate;
			}

			/// <summary>
			/// Recalculate current frame-rate based on the last frame-time.
			/// </summary>
			/// <param name="frametime">Time of last frame in nanoseconds.</param>
			void calculate(unsigned long long frametime)
			{
				_tick_sum -= _tick_list[_index];
				_tick_sum += _tick_list[_index++] = frametime;

				if (_index >= SAMPLES)
				{
					_index = 0;
				}

				_framerate = 1000000000.0f * SAMPLES / _tick_sum;
			}

		private:
			static const unsigned int SAMPLES = 100;
			float _framerate;
			unsigned long long _index, _tick_sum, _tick_list[SAMPLES];
		};
	}
}
