#include "GUI.hpp"
#include "Runtime.hpp"
#include "WindowWatcher.hpp"

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
	
	GUI::GUI(const Runtime *runtime, NVGcontext *context) : mRuntime(runtime), mNVG(context)
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

		this->mWindowX = 0;
		this->mWindowY = 0;
		this->mWindowWidth = static_cast<float>(this->mRuntime->GetBufferWidth());
		this->mWindowHeight = static_cast<float>(this->mRuntime->GetBufferHeight());

		nvgBeginFrame(this->mNVG, static_cast<int>(this->mWindowWidth), static_cast<int>(this->mWindowHeight), 1.0f);

		nvgFontFace(this->mNVG, "DefaultFont");

		return true;
	}
	void GUI::EndFrame()
	{
		nvgEndFrame(this->mNVG);
	}
	void GUI::BeginWindow(const std::string &title, float x, float y, float w, float h)
	{
		const float cornerRadius = 3.0f;

		this->mWindowX = x + 10;
		this->mWindowY = y + 35;
		this->mWindowWidth = w - 20;
		this->mWindowHeight = h;

		nvgSave(this->mNVG);

		// Window
		nvgBeginPath(this->mNVG);
		nvgRoundedRect(this->mNVG, x, y, w, h, cornerRadius);
		nvgFillColor(this->mNVG, nvgRGBA(28, 30, 34, 192));
		nvgFill(this->mNVG);

		// Drop shadow
		nvgBeginPath(this->mNVG);
		nvgRect(this->mNVG, x - 10, y - 10, w + 20, h + 30);
		nvgRoundedRect(this->mNVG, x, y, w, h, cornerRadius);
		nvgPathWinding(this->mNVG, NVG_HOLE);
		nvgFillPaint(this->mNVG, nvgBoxGradient(this->mNVG, x, y + 2, w, h, cornerRadius * 2, 10, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0)));
		nvgFill(this->mNVG);

		// Header
		nvgBeginPath(this->mNVG);
		nvgRoundedRect(this->mNVG, x + 1, y + 1, w - 2, 30, cornerRadius - 1);
		nvgFillPaint(this->mNVG, nvgLinearGradient(this->mNVG, x, y, x, y + 15, nvgRGBA(255, 255, 255, 8), nvgRGBA(0, 0, 0, 16)));
		nvgFill(this->mNVG);
		nvgBeginPath(this->mNVG);
		nvgMoveTo(this->mNVG, x + 0.5f, y + 0.5f + 30);
		nvgLineTo(this->mNVG, x + 0.5f + w - 1, y + 0.5f + 30);
		nvgStrokeColor(this->mNVG, nvgRGBA(0, 0, 0, 32));
		nvgStroke(this->mNVG);

		nvgFontSize(this->mNVG, 18.0f);
		//nvgFontFace(this->mNVG, "sans-bold");
		nvgTextAlign(this->mNVG, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		nvgFontBlur(this->mNVG, 2);
		nvgFillColor(this->mNVG, nvgRGBA(0, 0, 0, 128));
		nvgText(this->mNVG, x + w / 2, y + 16 + 1, title.c_str(), nullptr);

		nvgFontBlur(this->mNVG, 0);
		nvgFillColor(this->mNVG, nvgRGBA(220, 220, 220, 160));
		nvgText(this->mNVG, x + w / 2, y + 16, title.c_str(), nullptr);

		nvgRestore(this->mNVG);

		nvgSave(this->mNVG);
	}
	void GUI::EndWindow()
	{
		nvgRestore(this->mNVG);

		this->mWindowX = 0;
		this->mWindowY = 0;
		this->mWindowWidth = static_cast<float>(this->mRuntime->GetBufferWidth());
		this->mWindowHeight = static_cast<float>(this->mRuntime->GetBufferHeight());
	}

	void GUI::DrawDebugText(float x, float y, int alignx, float linewidth, int fontsize, unsigned int color, const std::string &text)
	{		
		if (alignx < 0)
			alignx = NVG_ALIGN_LEFT;
		else if (alignx == 0)
			alignx = NVG_ALIGN_CENTER;
		else if (alignx > 0)
			alignx = NVG_ALIGN_RIGHT;

		nvgSave(this->mNVG);

		nvgFontFace(this->mNVG, "DefaultFont");
		nvgFontSize(this->mNVG, static_cast<float>(fontsize));
		nvgFillColor(this->mNVG, nvgRGBA((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF));
		nvgTextAlign(this->mNVG, alignx | NVG_ALIGN_TOP);
		nvgTextBox(this->mNVG, x, y, linewidth, text.c_str(), nullptr);

		nvgRestore(this->mNVG);
	}

	void GUI::AddLabel(const std::string &label)
	{
		AddLabel(20.0f, 0x80FFFFFF, label);
	}
	void GUI::AddLabel(float h, unsigned int col, const std::string &label)
	{
		const float x = this->mWindowX, y = this->mWindowY, w = this->mWindowWidth;

		// Label
		nvgFontSize(this->mNVG, h);
		//nvgFontFace(this->mNVG, "sans");
		nvgFillColor(this->mNVG, nvgRGBA((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF, (col >> 24) & 0xFF));

		nvgTextAlign(this->mNVG, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgTextBox(this->mNVG, x, y, w, label.c_str(), nullptr);

		float bounds[4];
		nvgTextBoxBounds(this->mNVG, 0, 0, w, label.c_str(), nullptr, bounds);

		// Advance
		AddVerticalSpace(bounds[3] + 5.0f);
	}
	bool GUI::AddButton(const std::string &label)
	{
		return AddButton(28.0f, 0xFF000000, label);
	}
	bool GUI::AddButton(float h, unsigned int col, const std::string &label)
	{
		const float x = this->mWindowX, y = this->mWindowY, w = this->mWindowWidth;
		const float cornerRadius = 4.0f;

		const POINT mouse = this->mRuntime->GetWindow()->GetMousePosition();
		const bool hovered = (mouse.x >= x && mouse.x <= (x + w)) && (mouse.y >= y && mouse.y <= (y + h));
		const bool clicked1 = hovered && this->mRuntime->GetWindow()->GetMouseButtonState(0);
		const bool clicked2 = hovered && this->mRuntime->GetWindow()->GetMouseButtonJustReleased(0);

		if (clicked1)
		{
			col += (col & 0xFFFFFF) < 0x808080 ? 0x303030 : -0x303030;
		}
		else if (hovered)
		{
			col += (col & 0xFFFFFF) < 0x808080 ? 0x101010 : -0x101010;
		}

		// Button
		nvgBeginPath(this->mNVG);
		nvgRoundedRect(this->mNVG, x + 1, y + 1, w - 2, h - 2, cornerRadius - 1);

		if ((col & 0xFFFFFF) != 0)
		{
			nvgFillColor(this->mNVG, nvgRGBA((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF, (col >> 24) & 0xFF));
			nvgFill(this->mNVG);
		}

		nvgFillPaint(this->mNVG, nvgLinearGradient(this->mNVG, x, y, x, y + h, nvgRGBA(255, 255, 255, (col & 0xFFFFFF) == 0 ? 16 : 32), nvgRGBA(0, 0, 0, (col & 0xFFFFFF) == 0 ? 16 : 32)));
		nvgFill(this->mNVG);

		nvgBeginPath(this->mNVG);
		nvgRoundedRect(this->mNVG, x + 0.5f, y + 0.5f, w - 1, h - 1, cornerRadius - 0.5f);
		nvgStrokeColor(this->mNVG, nvgRGBA(0, 0, 0, 48));
		nvgStroke(this->mNVG);

		// Label
		nvgFontSize(this->mNVG, 20.0f);
		//nvgFontFace(this->mNVG, "sans-bold");

		const float tw = nvgTextBounds(this->mNVG, 0, 0, label.c_str(), nullptr, nullptr);

		nvgTextAlign(this->mNVG, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(this->mNVG, nvgRGBA(0, 0, 0, 160));
		nvgText(this->mNVG, x + w * 0.5f - tw * 0.5f, y + h * 0.5f - 1, label.c_str(), nullptr);
		nvgFillColor(this->mNVG, nvgRGBA(255, 255, 255, 160));
		nvgText(this->mNVG, x + w * 0.5f - tw * 0.5f, y + h * 0.5f, label.c_str(), nullptr);

		// Advance
		AddVerticalSpace(h + 5.0f);

		return clicked2;
	}
	void GUI::AddCheckBox(const std::string &label, bool &value)
	{
		const float x = this->mWindowX, y = this->mWindowY;
		const float w = this->mWindowWidth, h = 28.0f;

		const POINT mouse = this->mRuntime->GetWindow()->GetMousePosition();
		const bool hovered = (mouse.x >= x && mouse.x <= (x + w)) && (mouse.y >= y && mouse.y <= (y + h));
		const bool clicked = hovered && this->mRuntime->GetWindow()->GetMouseButtonJustReleased(0);

		if (clicked)
		{
			value = !value;
		}

		// Label
		nvgFontSize(this->mNVG, 18.0f);
		//nvgFontFace(this->mNVG, "sans");
		nvgFillColor(this->mNVG, nvgRGBA(255, 255, 255, 160));

		nvgTextAlign(this->mNVG, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(this->mNVG, x + 28, y + h * 0.5f, label.c_str(), nullptr);

		// Checkbox
		nvgBeginPath(this->mNVG);
		nvgRoundedRect(this->mNVG, x + 1, y + static_cast<int>(h * 0.5f) - 9, 18, 18, 3);
		nvgFillPaint(this->mNVG, nvgBoxGradient(this->mNVG, x + 1, y + static_cast<int>(h * 0.5f) - 9 + 1, 18, 18, 3, 3, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 92)));
		nvgFill(this->mNVG);

		if (value)
		{
			nvgBeginPath(this->mNVG);
			nvgRoundedRect(this->mNVG, x + 3, y + static_cast<int>(h * 0.5f) - 7, 14, 14, 3);
			nvgFillPaint(this->mNVG, nvgBoxGradient(this->mNVG, x + 3, y + static_cast<int>(h * 0.5f) - 7 + 1, 14, 14, 3, 3, nvgRGBA(255, 255, 255, 32), nvgRGBA(255, 255, 255, 92)));
			nvgFill(this->mNVG);
		}

		// Advance
		AddVerticalSpace(h + 5.0f);
	}
	void GUI::AddSliderInt(int &value, int min, int max)
	{
		const float x = this->mWindowX, y = this->mWindowY;
		const float w = this->mWindowWidth, h = 28.0f;
		const float cy = y + static_cast<float>(static_cast<int>(h * 0.5f)), kr = static_cast<float>(static_cast<int>(h * 0.25f));

		const POINT mouse = this->mRuntime->GetWindow()->GetMousePosition();
		const bool hovered = (mouse.x >= x && mouse.x <= (x + w)) && (mouse.y >= y && mouse.y <= (y + h));
		const bool clicked = hovered && this->mRuntime->GetWindow()->GetMouseButtonState(0);

		if (clicked)
		{
			const float value2 = (max - min) * (static_cast<float>(mouse.x) - x) / w;
			value = value2 > 0 ? static_cast<int>(value2 + 0.5f) : static_cast<int>(value2 - 0.5f);
		}

		if (value < min)
		{
			value = min;
		}
		else if (value > max)
		{
			value = max;
		}

		const float pos = static_cast<float>(value) / (max - min);

		nvgSave(this->mNVG);

		// Slot
		nvgBeginPath(this->mNVG);
		nvgRoundedRect(this->mNVG, x, cy - 2, w, 4, 2);
		nvgFillPaint(this->mNVG, nvgBoxGradient(this->mNVG, x, cy - 2 + 1, w, 4, 2, 2, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 128)));
		nvgFill(this->mNVG);

		// Knob Shadow
		nvgBeginPath(this->mNVG);
		nvgRect(this->mNVG, x + static_cast<int>(pos * w) - kr - 5, cy - kr - 5, kr * 2 + 5 + 5, kr * 2 + 5 + 5 + 3);
		nvgCircle(this->mNVG, x + static_cast<int>(pos * w), cy, kr);
		nvgPathWinding(this->mNVG, NVG_HOLE);
		nvgFillPaint(this->mNVG, nvgRadialGradient(this->mNVG, x + static_cast<int>(pos * w), cy + 1, kr - 3, kr + 3, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0)));
		nvgFill(this->mNVG);

		// Knob
		nvgBeginPath(this->mNVG);
		nvgCircle(this->mNVG, x + static_cast<int>(pos * w), cy, kr - 1);
		nvgFillColor(this->mNVG, nvgRGBA(40, 43, 48, 255));
		nvgFill(this->mNVG);
		nvgFillPaint(this->mNVG, nvgLinearGradient(this->mNVG, x, cy - kr, x, cy + kr, nvgRGBA(255, 255, 255, 16), nvgRGBA(0, 0, 0, 16)));
		nvgFill(this->mNVG);

		nvgBeginPath(this->mNVG);
		nvgCircle(this->mNVG, x + static_cast<int>(pos * w), cy, kr - 0.5f);
		nvgStrokeColor(this->mNVG, nvgRGBA(0, 0, 0, 92));
		nvgStroke(this->mNVG);

		nvgRestore(this->mNVG);

		// Advance
		AddVerticalSpace(h + 5.0f);
	}
	void GUI::AddSliderFloat(float &value, float min, float max)
	{
		const float x = this->mWindowX, y = this->mWindowY;
		const float w = this->mWindowWidth, h = 28.0f;
		const float cy = y + static_cast<float>(static_cast<int>(h * 0.5f)), kr = static_cast<float>(static_cast<int>(h * 0.25f));

		const POINT mouse = this->mRuntime->GetWindow()->GetMousePosition();
		const bool hovered = (mouse.x >= x && mouse.x <= (x + w)) && (mouse.y >= y && mouse.y <= (y + h));
		const bool clicked = hovered && this->mRuntime->GetWindow()->GetMouseButtonState(0);

		if (clicked)
		{
			value = (max - min) * (static_cast<float>(mouse.x) - x) / w;
		}

		if (value < min)
		{
			value = min;
		}
		else if (value > max)
		{
			value = max;
		}

		const float pos = value / (max - min);

		nvgSave(this->mNVG);

		// Slot
		nvgBeginPath(this->mNVG);
		nvgRoundedRect(this->mNVG, x, cy - 2, w, 4, 2);
		nvgFillPaint(this->mNVG, nvgBoxGradient(this->mNVG, x, cy - 2 + 1, w, 4, 2, 2, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 128)));
		nvgFill(this->mNVG);

		// Knob Shadow
		nvgBeginPath(this->mNVG);
		nvgRect(this->mNVG, x + static_cast<int>(pos * w) - kr - 5, cy - kr - 5, kr * 2 + 5 + 5, kr * 2 + 5 + 5 + 3);
		nvgCircle(this->mNVG, x + static_cast<int>(pos * w), cy, kr);
		nvgPathWinding(this->mNVG, NVG_HOLE);
		nvgFillPaint(this->mNVG, nvgRadialGradient(this->mNVG, x + static_cast<int>(pos * w), cy + 1, kr - 3, kr + 3, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0)));
		nvgFill(this->mNVG);

		// Knob
		nvgBeginPath(this->mNVG);
		nvgCircle(this->mNVG, x + static_cast<int>(pos * w), cy, kr - 1);
		nvgFillColor(this->mNVG, nvgRGBA(40, 43, 48, 255));
		nvgFill(this->mNVG);
		nvgFillPaint(this->mNVG, nvgLinearGradient(this->mNVG, x, cy - kr, x, cy + kr, nvgRGBA(255, 255, 255, 16), nvgRGBA(0, 0, 0, 16)));
		nvgFill(this->mNVG);

		nvgBeginPath(this->mNVG);
		nvgCircle(this->mNVG, x + static_cast<int>(pos * w), cy, kr - 0.5f);
		nvgStrokeColor(this->mNVG, nvgRGBA(0, 0, 0, 92));
		nvgStroke(this->mNVG);

		nvgRestore(this->mNVG);

		// Advance
		AddVerticalSpace(h + 5.0f);
	}
	void GUI::AddVerticalSpace(float height)
	{
		this->mWindowY += height;
	}
}
