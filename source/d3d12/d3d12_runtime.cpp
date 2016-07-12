#include "log.hpp"
#include "d3d12_runtime.hpp"
#include "input.hpp"

#include <assert.h>

namespace reshade
{
	d3d12_runtime::d3d12_runtime(ID3D12Device *device, ID3D12CommandQueue *queue, IDXGISwapChain3 *swapchain) : runtime(D3D_FEATURE_LEVEL_12_0), _device(device), _commandqueue(queue), _swapchain(swapchain)
	{
		assert(queue != nullptr);
		assert(device != nullptr);
		assert(swapchain != nullptr);
	}

	bool d3d12_runtime::on_init(const DXGI_SWAP_CHAIN_DESC &desc)
	{
		_width = desc.BufferDesc.Width;
		_height = desc.BufferDesc.Height;
		_input = input::register_window(desc.OutputWindow);

		return runtime::on_init();
	}
	void d3d12_runtime::on_reset()
	{
		if (!_is_initialized)
		{
			return;
		}

		runtime::on_reset();
	}
	void d3d12_runtime::on_reset_effect()
	{
		runtime::on_reset_effect();
	}
	void d3d12_runtime::on_present()
	{
		if (!_is_initialized)
		{
			LOG(TRACE) << "Failed to present! Runtime is in a lost state.";
			return;
		}
		else if (_drawcalls == 0)
		{
			return;
		}

		runtime::on_present_effect();
		runtime::on_present();
	}

	void d3d12_runtime::capture_frame(uint8_t *buffer) const
	{
	}
	bool d3d12_runtime::update_effect(const reshadefx::syntax_tree &ast, std::string &errors)
	{
		return false;
	}
	bool d3d12_runtime::update_texture(texture &texture, const uint8_t *data)
	{
		return false;
	}
	bool d3d12_runtime::update_texture_reference(texture &texture, unsigned short id)
	{
		return false;
	}

	void d3d12_runtime::render_technique(const technique &technique)
	{
	}
	void d3d12_runtime::render_draw_lists(ImDrawData *draw_data)
	{
	}
}
