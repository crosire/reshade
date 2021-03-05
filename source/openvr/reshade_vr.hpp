#pragma once

#include "reshade_api.hpp"

namespace reshade::vr
{
	struct submit_info
	{
		static constexpr uint8_t GUID[16] = { 0x20, 0x98, 0x62, 0x08, 0xA2, 0x5E, 0x47, 0x43, 0xBC, 0x2B, 0x42, 0xF2, 0xA6, 0x23, 0xBB, 0xB6 };

		uint8_t eye = 0;
		api::resource_handle color = {0};
		api::resource_handle depth = {0};
		api::region region;
	};
}
