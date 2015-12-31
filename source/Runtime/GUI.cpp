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
	
	GUI::GUI(const Runtime *runtime, NVGcontext *context) : _runtime(runtime), _nvg(context)
	{
		nvgCreateFont(_nvg, "DefaultFont", (GetWindowsDirectory() / "Fonts" / "courbd.ttf").string().c_str());
	}
	GUI::~GUI()
	{
	}

	bool GUI::BeginFrame()
	{
		if (_nvg == nullptr)
		{
			return false;
		}

		_windowX = 0;
		_windowY = 0;
		_windowWidth = static_cast<float>(_runtime->GetBufferWidth());
		_windowHeight = static_cast<float>(_runtime->GetBufferHeight());

		nvgBeginFrame(_nvg, static_cast<int>(_windowWidth), static_cast<int>(_windowHeight), 1.0f);

		nvgFontFace(_nvg, "DefaultFont");

		return true;
	}
	void GUI::EndFrame()
	{
		nvgEndFrame(_nvg);
	}
	void GUI::BeginWindow(const std::string &title, float x, float y, float w, float h)
	{
		const float cornerRadius = 3.0f;

		_windowX = x + 10;
		_windowY = y + 35;
		_windowWidth = w - 20;
		_windowHeight = h;

		nvgSave(_nvg);

		// Window
		nvgBeginPath(_nvg);
		nvgRoundedRect(_nvg, x, y, w, h, cornerRadius);
		nvgFillColor(_nvg, nvgRGBA(28, 30, 34, 192));
		nvgFill(_nvg);

		// Drop shadow
		nvgBeginPath(_nvg);
		nvgRect(_nvg, x - 10, y - 10, w + 20, h + 30);
		nvgRoundedRect(_nvg, x, y, w, h, cornerRadius);
		nvgPathWinding(_nvg, NVG_HOLE);
		nvgFillPaint(_nvg, nvgBoxGradient(_nvg, x, y + 2, w, h, cornerRadius * 2, 10, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0)));
		nvgFill(_nvg);

		// Header
		nvgBeginPath(_nvg);
		nvgRoundedRect(_nvg, x + 1, y + 1, w - 2, 30, cornerRadius - 1);
		nvgFillPaint(_nvg, nvgLinearGradient(_nvg, x, y, x, y + 15, nvgRGBA(255, 255, 255, 8), nvgRGBA(0, 0, 0, 16)));
		nvgFill(_nvg);
		nvgBeginPath(_nvg);
		nvgMoveTo(_nvg, x + 0.5f, y + 0.5f + 30);
		nvgLineTo(_nvg, x + 0.5f + w - 1, y + 0.5f + 30);
		nvgStrokeColor(_nvg, nvgRGBA(0, 0, 0, 32));
		nvgStroke(_nvg);

		nvgFontSize(_nvg, 18.0f);
		//nvgFontFace(_nvg, "sans-bold");
		nvgTextAlign(_nvg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

		nvgFontBlur(_nvg, 2);
		nvgFillColor(_nvg, nvgRGBA(0, 0, 0, 128));
		nvgText(_nvg, x + w / 2, y + 16 + 1, title.c_str(), nullptr);

		nvgFontBlur(_nvg, 0);
		nvgFillColor(_nvg, nvgRGBA(220, 220, 220, 160));
		nvgText(_nvg, x + w / 2, y + 16, title.c_str(), nullptr);

		nvgRestore(_nvg);

		nvgSave(_nvg);
	}
	void GUI::EndWindow()
	{
		nvgRestore(_nvg);

		_windowX = 0;
		_windowY = 0;
		_windowWidth = static_cast<float>(_runtime->GetBufferWidth());
		_windowHeight = static_cast<float>(_runtime->GetBufferHeight());
	}

	void GUI::DrawDebugText(float x, float y, int alignx, float linewidth, int fontsize, unsigned int color, const std::string &text)
	{		
		if (alignx < 0)
			alignx = NVG_ALIGN_LEFT;
		else if (alignx == 0)
			alignx = NVG_ALIGN_CENTER;
		else if (alignx > 0)
			alignx = NVG_ALIGN_RIGHT;

		nvgSave(_nvg);

		nvgFontFace(_nvg, "DefaultFont");
		nvgFontSize(_nvg, static_cast<float>(fontsize));
		nvgFillColor(_nvg, nvgRGBA((color >> 16) & 0xFF, (color >> 8) & 0xFF, color & 0xFF, (color >> 24) & 0xFF));
		nvgTextAlign(_nvg, alignx | NVG_ALIGN_TOP);
		nvgTextBox(_nvg, x, y, linewidth, text.c_str(), nullptr);

		nvgRestore(_nvg);
	}

	void GUI::AddLabel(const std::string &label)
	{
		AddLabel(20.0f, 0x80FFFFFF, label);
	}
	void GUI::AddLabel(float h, unsigned int col, const std::string &label)
	{
		const float x = _windowX, y = _windowY, w = _windowWidth;

		// Label
		nvgFontSize(_nvg, h);
		//nvgFontFace(_nvg, "sans");
		nvgFillColor(_nvg, nvgRGBA((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF, (col >> 24) & 0xFF));

		nvgTextAlign(_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
		nvgTextBox(_nvg, x, y, w, label.c_str(), nullptr);

		float bounds[4];
		nvgTextBoxBounds(_nvg, 0, 0, w, label.c_str(), nullptr, bounds);

		// Advance
		AddVerticalSpace(bounds[3] + 5.0f);
	}
	bool GUI::AddButton(const std::string &label)
	{
		return AddButton(28.0f, 0xFF000000, label);
	}
	bool GUI::AddButton(float h, unsigned int col, const std::string &label)
	{
		const float x = _windowX, y = _windowY, w = _windowWidth;
		const float cornerRadius = 4.0f;

		const POINT mouse = _runtime->GetWindow()->GetMousePosition();
		const bool hovered = (mouse.x >= x && mouse.x <= (x + w)) && (mouse.y >= y && mouse.y <= (y + h));
		const bool clicked1 = hovered && _runtime->GetWindow()->GetMouseButtonState(0);
		const bool clicked2 = hovered && _runtime->GetWindow()->GetMouseButtonJustReleased(0);

		if (clicked1)
		{
			col += (col & 0xFFFFFF) < 0x808080 ? 0x303030 : -0x303030;
		}
		else if (hovered)
		{
			col += (col & 0xFFFFFF) < 0x808080 ? 0x101010 : -0x101010;
		}

		// Button
		nvgBeginPath(_nvg);
		nvgRoundedRect(_nvg, x + 1, y + 1, w - 2, h - 2, cornerRadius - 1);

		if ((col & 0xFFFFFF) != 0)
		{
			nvgFillColor(_nvg, nvgRGBA((col >> 16) & 0xFF, (col >> 8) & 0xFF, col & 0xFF, (col >> 24) & 0xFF));
			nvgFill(_nvg);
		}

		nvgFillPaint(_nvg, nvgLinearGradient(_nvg, x, y, x, y + h, nvgRGBA(255, 255, 255, (col & 0xFFFFFF) == 0 ? 16 : 32), nvgRGBA(0, 0, 0, (col & 0xFFFFFF) == 0 ? 16 : 32)));
		nvgFill(_nvg);

		nvgBeginPath(_nvg);
		nvgRoundedRect(_nvg, x + 0.5f, y + 0.5f, w - 1, h - 1, cornerRadius - 0.5f);
		nvgStrokeColor(_nvg, nvgRGBA(0, 0, 0, 48));
		nvgStroke(_nvg);

		// Label
		nvgFontSize(_nvg, 20.0f);
		//nvgFontFace(_nvg, "sans-bold");

		const float tw = nvgTextBounds(_nvg, 0, 0, label.c_str(), nullptr, nullptr);

		nvgTextAlign(_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgFillColor(_nvg, nvgRGBA(0, 0, 0, 160));
		nvgText(_nvg, x + w * 0.5f - tw * 0.5f, y + h * 0.5f - 1, label.c_str(), nullptr);
		nvgFillColor(_nvg, nvgRGBA(255, 255, 255, 160));
		nvgText(_nvg, x + w * 0.5f - tw * 0.5f, y + h * 0.5f, label.c_str(), nullptr);

		// Advance
		AddVerticalSpace(h + 5.0f);

		return clicked2;
	}
	void GUI::AddCheckBox(const std::string &label, bool &value)
	{
		const float x = _windowX, y = _windowY;
		const float w = _windowWidth, h = 28.0f;

		const POINT mouse = _runtime->GetWindow()->GetMousePosition();
		const bool hovered = (mouse.x >= x && mouse.x <= (x + w)) && (mouse.y >= y && mouse.y <= (y + h));
		const bool clicked = hovered && _runtime->GetWindow()->GetMouseButtonJustReleased(0);

		if (clicked)
		{
			value = !value;
		}

		// Label
		nvgFontSize(_nvg, 18.0f);
		//nvgFontFace(_nvg, "sans");
		nvgFillColor(_nvg, nvgRGBA(255, 255, 255, 160));

		nvgTextAlign(_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
		nvgText(_nvg, x + 28, y + h * 0.5f, label.c_str(), nullptr);

		// Checkbox
		nvgBeginPath(_nvg);
		nvgRoundedRect(_nvg, x + 1, y + static_cast<int>(h * 0.5f) - 9, 18, 18, 3);
		nvgFillPaint(_nvg, nvgBoxGradient(_nvg, x + 1, y + static_cast<int>(h * 0.5f) - 9 + 1, 18, 18, 3, 3, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 92)));
		nvgFill(_nvg);

		if (value)
		{
			nvgBeginPath(_nvg);
			nvgRoundedRect(_nvg, x + 3, y + static_cast<int>(h * 0.5f) - 7, 14, 14, 3);
			nvgFillPaint(_nvg, nvgBoxGradient(_nvg, x + 3, y + static_cast<int>(h * 0.5f) - 7 + 1, 14, 14, 3, 3, nvgRGBA(255, 255, 255, 32), nvgRGBA(255, 255, 255, 92)));
			nvgFill(_nvg);
		}

		// Advance
		AddVerticalSpace(h + 5.0f);
	}
	void GUI::AddSliderInt(int &value, int min, int max)
	{
		const float x = _windowX, y = _windowY;
		const float w = _windowWidth, h = 28.0f;
		const float cy = y + static_cast<float>(static_cast<int>(h * 0.5f)), kr = static_cast<float>(static_cast<int>(h * 0.25f));

		const POINT mouse = _runtime->GetWindow()->GetMousePosition();
		const bool hovered = (mouse.x >= x && mouse.x <= (x + w)) && (mouse.y >= y && mouse.y <= (y + h));
		const bool clicked = hovered && _runtime->GetWindow()->GetMouseButtonState(0);

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

		nvgSave(_nvg);

		// Slot
		nvgBeginPath(_nvg);
		nvgRoundedRect(_nvg, x, cy - 2, w, 4, 2);
		nvgFillPaint(_nvg, nvgBoxGradient(_nvg, x, cy - 2 + 1, w, 4, 2, 2, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 128)));
		nvgFill(_nvg);

		// Knob Shadow
		nvgBeginPath(_nvg);
		nvgRect(_nvg, x + static_cast<int>(pos * w) - kr - 5, cy - kr - 5, kr * 2 + 5 + 5, kr * 2 + 5 + 5 + 3);
		nvgCircle(_nvg, x + static_cast<int>(pos * w), cy, kr);
		nvgPathWinding(_nvg, NVG_HOLE);
		nvgFillPaint(_nvg, nvgRadialGradient(_nvg, x + static_cast<int>(pos * w), cy + 1, kr - 3, kr + 3, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0)));
		nvgFill(_nvg);

		// Knob
		nvgBeginPath(_nvg);
		nvgCircle(_nvg, x + static_cast<int>(pos * w), cy, kr - 1);
		nvgFillColor(_nvg, nvgRGBA(40, 43, 48, 255));
		nvgFill(_nvg);
		nvgFillPaint(_nvg, nvgLinearGradient(_nvg, x, cy - kr, x, cy + kr, nvgRGBA(255, 255, 255, 16), nvgRGBA(0, 0, 0, 16)));
		nvgFill(_nvg);

		nvgBeginPath(_nvg);
		nvgCircle(_nvg, x + static_cast<int>(pos * w), cy, kr - 0.5f);
		nvgStrokeColor(_nvg, nvgRGBA(0, 0, 0, 92));
		nvgStroke(_nvg);

		nvgRestore(_nvg);

		// Advance
		AddVerticalSpace(h + 5.0f);
	}
	void GUI::AddSliderFloat(float &value, float min, float max)
	{
		const float x = _windowX, y = _windowY;
		const float w = _windowWidth, h = 28.0f;
		const float cy = y + static_cast<float>(static_cast<int>(h * 0.5f)), kr = static_cast<float>(static_cast<int>(h * 0.25f));

		const POINT mouse = _runtime->GetWindow()->GetMousePosition();
		const bool hovered = (mouse.x >= x && mouse.x <= (x + w)) && (mouse.y >= y && mouse.y <= (y + h));
		const bool clicked = hovered && _runtime->GetWindow()->GetMouseButtonState(0);

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

		nvgSave(_nvg);

		// Slot
		nvgBeginPath(_nvg);
		nvgRoundedRect(_nvg, x, cy - 2, w, 4, 2);
		nvgFillPaint(_nvg, nvgBoxGradient(_nvg, x, cy - 2 + 1, w, 4, 2, 2, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 128)));
		nvgFill(_nvg);

		// Knob Shadow
		nvgBeginPath(_nvg);
		nvgRect(_nvg, x + static_cast<int>(pos * w) - kr - 5, cy - kr - 5, kr * 2 + 5 + 5, kr * 2 + 5 + 5 + 3);
		nvgCircle(_nvg, x + static_cast<int>(pos * w), cy, kr);
		nvgPathWinding(_nvg, NVG_HOLE);
		nvgFillPaint(_nvg, nvgRadialGradient(_nvg, x + static_cast<int>(pos * w), cy + 1, kr - 3, kr + 3, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0)));
		nvgFill(_nvg);

		// Knob
		nvgBeginPath(_nvg);
		nvgCircle(_nvg, x + static_cast<int>(pos * w), cy, kr - 1);
		nvgFillColor(_nvg, nvgRGBA(40, 43, 48, 255));
		nvgFill(_nvg);
		nvgFillPaint(_nvg, nvgLinearGradient(_nvg, x, cy - kr, x, cy + kr, nvgRGBA(255, 255, 255, 16), nvgRGBA(0, 0, 0, 16)));
		nvgFill(_nvg);

		nvgBeginPath(_nvg);
		nvgCircle(_nvg, x + static_cast<int>(pos * w), cy, kr - 0.5f);
		nvgStrokeColor(_nvg, nvgRGBA(0, 0, 0, 92));
		nvgStroke(_nvg);

		nvgRestore(_nvg);

		// Advance
		AddVerticalSpace(h + 5.0f);
	}
	void GUI::AddVerticalSpace(float height)
	{
		_windowY += height;
	}
}
