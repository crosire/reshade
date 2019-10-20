/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <vulkan.h>

inline VkFormat make_format_srgb(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_R8G8B8A8_UNORM:
		return VK_FORMAT_R8G8B8A8_SRGB;
	case VK_FORMAT_B8G8R8A8_UNORM:
		return VK_FORMAT_B8G8R8A8_SRGB;
	default:
		return format;
	}
}

inline VkFormat make_format_normal(VkFormat format)
{
	switch (format)
	{
	case VK_FORMAT_R8G8B8A8_SRGB:
		return VK_FORMAT_R8G8B8A8_UNORM;
	case VK_FORMAT_B8G8R8A8_SRGB:
		return VK_FORMAT_B8G8R8A8_UNORM;
	default:
		return format;
	}
}

inline VkImageAspectFlags aspect_flags_from_format(VkFormat format)
{
	if (format == VK_FORMAT_D16_UNORM || format == VK_FORMAT_D32_SFLOAT)
		return VK_IMAGE_ASPECT_DEPTH_BIT;
	if (format >= VK_FORMAT_D16_UNORM_S8_UINT && format <= VK_FORMAT_D32_SFLOAT_S8_UINT)
		return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
	return VK_IMAGE_ASPECT_COLOR_BIT;
}
