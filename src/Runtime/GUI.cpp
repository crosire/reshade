#include "GUI.hpp"

#include <nanovg.h>
#include <boost\filesystem\path.hpp>
#include <Windows.h>

namespace ReShade
{
	namespace
	{
		inline boost::filesystem::path GetWindowsDirectory()
		{
			TCHAR path[MAX_PATH];
			::GetWindowsDirectory(path, MAX_PATH);
			return path;
		}
	}
	
	GUI::GUI(NVGcontext *context, unsigned int width, unsigned int height) : mNVG(context), mWidth(width), mHeight(height)
	{
		nvgCreateFont(this->mNVG, "DefaultFont", (GetWindowsDirectory() / "Fonts" / "courbd.ttf").string().c_str());
	}
	GUI::~GUI()
	{
	}

	bool GUI::BeginFrame()
	{
		if (this->mNVG == nullptr)
		{
			return false;
		}

		nvgBeginFrame(this->mNVG, this->mWidth, this->mHeight, 1.0f);

		SetFont(Font::Default);
		SetAlignment(-1, -1);

		return true;
	}
	void GUI::FlushFrame()
	{
		nvgEndFrame(this->mNVG);
	}

	void GUI::SetFont(Font font)
	{
		switch (font)
		{
			case Font::Default:
				nvgFontFace(this->mNVG, "DefaultFont");
				break;
		}
	}
	void GUI::SetAlignment(int x, int y)
	{
		int align = 0;

		if (x < 0)
			align |= NVG_ALIGN_LEFT;
		else if (x == 0)
			align |= NVG_ALIGN_CENTER;
		else if (x > 0)
			align |= NVG_ALIGN_RIGHT;

		if (y < 0)
			align |= NVG_ALIGN_TOP;
		else if (y == 0)
			align |= NVG_ALIGN_MIDDLE;
		else if (y > 0)
			align |= NVG_ALIGN_BOTTOM;

		nvgTextAlign(this->mNVG, align);

		this->mCurrentAlignmentY = static_cast<char>(y);
	}

	void GUI::DrawSimpleText(float x, float y, int fontsize, unsigned int color, const std::string &text)
	{
		nvgFontSize(this->mNVG, static_cast<float>(fontsize));
		nvgFillColor(this->mNVG, nvgRGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
		nvgTextAlign(this->mNVG, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgText(this->mNVG, x, y, text.c_str(), nullptr);
	}
	void GUI::DrawSimpleTextMultiLine(float x, float y, float linewidth, int fontsize, unsigned int color, const std::string &text)
	{		
		if (this->mCurrentAlignmentY == 0)
		{
			float bounds[4];
			nvgTextBoxBounds(this->mNVG, 0, 0, static_cast<float>(this->mWidth), text.c_str(), nullptr, bounds);

			y += static_cast<float>(this->mHeight) / 2 - bounds[3] / 2;
		}

		nvgFontSize(this->mNVG, static_cast<float>(fontsize));
		nvgFillColor(this->mNVG, nvgRGB((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF));
		nvgTextBox(this->mNVG, x, y, linewidth, text.c_str(), nullptr);
	}
}
