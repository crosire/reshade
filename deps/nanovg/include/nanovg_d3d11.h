//
// Copyright (c) 2009-2013 Mikko Mononen memon@inside.org
// Port of _gl.h to _d3d11.h by Chris Maughan
//
// This software is provided 'as-is', without any express or implied
// warranty.  In no event will the authors be held liable for any damages
// arising from the use of this software.
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it
// freely, subject to the following restrictions:
// 1. The origin of this software must not be misrepresented; you must not
//    claim that you wrote the original software. If you use this software
//    in a product, an acknowledgment in the product documentation would be
//    appreciated but is not required.
// 2. Altered source versions must be plainly marked as such, and must not be
//    misrepresented as being the original software.
// 3. This notice may not be removed or altered from any source distribution.
//
#ifndef NANOVG_D3D11_H
#define NANOVG_D3D11_H

#ifdef __cplusplus
extern "C" {
#endif

struct NVGcontext* nvgCreateD3D11(ID3D11Device* pDevice, int edgeaa);
void nvgDeleteD3D11(struct NVGcontext* ctx);

// These are additional flags on top of NVGimageFlags.
enum NVGimageFlagsD3D11 {
	NVG_IMAGE_NODELETE			= 1<<16,	// Do not delete texture object.
};

// Not done yet.  Simple enough to do though...
#ifdef IMPLEMENTED_IMAGE_FUNCS
int nvd3dCreateImageFromHandle(struct NVGcontext* ctx, void* texture, int w, int h, int flags);
unsigned int nvd3dImageHandle(struct NVGcontext* ctx, int image);
void nvd3dImageFlags(struct NVGcontext* ctx, int image, int flags);
#endif

#ifdef __cplusplus
}
#endif

#endif //NANOVG_D3D11_H