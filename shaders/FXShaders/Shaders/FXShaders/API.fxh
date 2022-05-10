#pragma once

#define FXSHADERS_API_D3D9 0x9000
#define FXSHADERS_API_D3D10 0xA000
#define FXSHADERS_API_D3D11 0xB000
#define FXSHADERS_API_D3D12 0xC000
#define FXSHADERS_API_OPENGL 0x10000
#define FXSHADERS_API_VULKAN 0x20000

/**
 * Check whether the game is running on a certain API.
 *
 * @param value The constant identifier value of the API.
 */
#define FXSHADERS_API_IS(value) ((__RENDERER__ & value) != 0)
