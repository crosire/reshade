/*
 * Copyright (C) 2023 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d11.h>

void STDMETHODCALLTYPE ID3D11Resource_GetDevice(ID3D11Resource *pResource, ID3D11Device **ppDevice);
