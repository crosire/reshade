#pragma once

#include <string>

#pragma region Forward Declarations
struct NVGcontext;

namespace reshade
{
	class runtime;
}
#pragma endregion

namespace reshade
{
	class gui
	{
	public:
		gui(const runtime *runtime, NVGcontext *context);

		NVGcontext *context() const
		{
			return _nvg;
		}

		bool begin_frame();
		void end_frame();
		void begin_window(const std::string &title, float x, float y, float width, float height);
		void end_window();

		void draw_debug_text(float x, float y, int alignx, float linewidth, int fontsize, unsigned int color, const std::string &text);

		void add_label(const std::string &label);
		void add_label(float lineheight, unsigned int color, const std::string &label);
		bool add_button(const std::string &label);
		bool add_button(float height, unsigned int color, const std::string &label);
		void add_checkbox(const std::string &label, bool &value);
		void add_slider_int(int &value, int min, int max);
		void add_slider_float(float &value, float min, float max);
		void add_vertical_space(float height);

	private:
		const runtime *_runtime;
		NVGcontext *_nvg;
		float _window_x, _window_y, _window_w, _window_h;
	};
}
