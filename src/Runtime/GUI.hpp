#pragma once

#include <string>

struct NVGcontext;

namespace ReShade
{
	class GUI
	{
	public:
		enum class Font
		{
			Default
		};

		GUI(NVGcontext *context, unsigned int width, unsigned int height);
		~GUI();

		inline NVGcontext *GetContext() const
		{
			return this->mNVG;
		}

		bool BeginFrame();
		void FlushFrame();

		void SetFont(Font font);
		void SetAlignment(int x, int y);

		void DrawSimpleText(float x, float y, int fontsize, unsigned int color, const std::string &text);
		void DrawSimpleTextMultiLine(float x, float y, float linewidth, int fontsize, unsigned int color, const std::string &text);

	private:
		NVGcontext *mNVG;
		unsigned int mWidth, mHeight;
		char mCurrentAlignmentY;
	};
}
