#pragma once

#include <string>

#pragma region Forward Declarations
struct NVGcontext;

namespace ReShade
{
	class Runtime;
}
#pragma endregion

namespace ReShade
{
	class GUI
	{
	public:
		GUI(const Runtime *runtime, NVGcontext *context);
		~GUI();

		NVGcontext *GetContext() const
		{
			return _nvg;
		}

		bool BeginFrame();
		void EndFrame();
		void BeginWindow(const std::string &title, float x, float y, float width, float height);
		void EndWindow();

		void DrawDebugText(float x, float y, int alignx, float linewidth, int fontsize, unsigned int color, const std::string &text);

		void AddLabel(const std::string &label);
		void AddLabel(float lineheight, unsigned int color, const std::string &label);
		bool AddButton(const std::string &label);
		bool AddButton(float height, unsigned int color, const std::string &label);
		void AddCheckBox(const std::string &label, bool &value);
		void AddSliderInt(int &value, int min, int max);
		void AddSliderFloat(float &value, float min, float max);
		void AddVerticalSpace(float height);

	private:
		const Runtime *_runtime;
		NVGcontext *_nvg;
		float _windowX, _windowY, _windowWidth, _windowHeight;
	};
}
