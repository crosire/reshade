/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

namespace reshade::d3d9
{
	void convert_memory_usage_to_d3d_pool(api::memory_usage usage, D3DPOOL &d3d_pool);
	void convert_d3d_pool_to_memory_usage(D3DPOOL d3d_pool, api::memory_usage &usage);

	void convert_resource_usage_to_d3d_usage(api::resource_usage usage, DWORD &d3d_usage);
	void convert_d3d_usage_to_resource_usage(DWORD d3d_usage, api::resource_usage &usage);

	void convert_resource_desc(const api::resource_desc &desc, D3DVOLUME_DESC &internal_desc, UINT *levels = nullptr);
	void convert_resource_desc(const api::resource_desc &desc, D3DSURFACE_DESC &internal_desc, UINT *levels = nullptr);
	void convert_resource_desc(const api::resource_desc &desc, D3DINDEXBUFFER_DESC &internal_desc);
	void convert_resource_desc(const api::resource_desc &desc, D3DVERTEXBUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3DVOLUME_DESC &internal_desc, UINT levels = 1);
	api::resource_desc convert_resource_desc(const D3DSURFACE_DESC &internal_desc, UINT levels, const D3DCAPS9 &caps);
	api::resource_desc convert_resource_desc(const D3DINDEXBUFFER_DESC &internal_desc);
	api::resource_desc convert_resource_desc(const D3DVERTEXBUFFER_DESC &internal_desc);
}
