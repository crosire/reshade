#pragma once

namespace ReShade
{
	namespace Utils
	{
		class Framerate
		{
		public:
			Framerate() : mIndex(0), mTickSum(0), mTickList()
			{
			}

			inline operator float() const
			{
				return this->mFramerate;
			}

			/// <summary>
			/// Recalculate current frame-rate based on the last frame-time.
			/// </summary>
			/// <param name="frametime">Time of last frame in nanoseconds.</param>
			void Calculate(unsigned long long frametime)
			{
				this->mTickSum -= this->mTickList[this->mIndex];
				this->mTickSum += this->mTickList[this->mIndex++] = frametime;

				if (this->mIndex == SAMPLES)
				{
					this->mIndex = 0;
				}

				this->mFramerate = 1000000000.0f * SAMPLES / this->mTickSum;
			}

		private:
			static const unsigned int SAMPLES = 100;
			float mFramerate;
			unsigned long long mIndex, mTickSum, mTickList[SAMPLES];
		};
	}
}
