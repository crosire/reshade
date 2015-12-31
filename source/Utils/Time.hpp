#pragma once

namespace ReShade
{
	namespace Utils
	{
		class Framerate
		{
		public:
			Framerate() : _index(0), _tickSum(0), _tickList()
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
			void Calculate(unsigned long long frametime)
			{
				_tickSum -= _tickList[_index];
				_tickSum += _tickList[_index++] = frametime;

				if (_index >= SAMPLES)
				{
					_index = 0;
				}

				_framerate = 1000000000.0f * SAMPLES / _tickSum;
			}

		private:
			static const unsigned int SAMPLES = 100;
			float _framerate;
			unsigned long long _index, _tickSum, _tickList[SAMPLES];
		};
	}
}
