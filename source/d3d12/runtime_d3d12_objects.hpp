/*
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "runtime_objects.hpp"

namespace reshade::d3d12
{
	struct tex_data
	{
		com_ptr<ID3D12Resource> resource;
		com_ptr<ID3D12DescriptorHeap> descriptors;
	};

	struct pass_data
	{
		com_ptr<ID3D12PipelineState> pipeline;
		UINT num_render_targets;
		D3D12_GPU_DESCRIPTOR_HANDLE srv_handle;
		D3D12_GPU_DESCRIPTOR_HANDLE uav_handle;
		D3D12_CPU_DESCRIPTOR_HANDLE render_targets;
		std::vector<const tex_data *> modified_resources;
	};

	struct effect_data
	{
		com_ptr<ID3D12Resource> cb;
		com_ptr<ID3D12RootSignature> signature;
		com_ptr<ID3D12DescriptorHeap> rtv_heap;
		com_ptr<ID3D12DescriptorHeap> srv_uav_heap;
		com_ptr<ID3D12DescriptorHeap> sampler_heap;

		D3D12_GPU_VIRTUAL_ADDRESS cbv_gpu_address;
		D3D12_CPU_DESCRIPTOR_HANDLE srv_cpu_base;
		D3D12_GPU_DESCRIPTOR_HANDLE srv_gpu_base;
		D3D12_CPU_DESCRIPTOR_HANDLE rtv_cpu_base;
		D3D12_CPU_DESCRIPTOR_HANDLE uav_cpu_base;
		D3D12_GPU_DESCRIPTOR_HANDLE uav_gpu_base;
		D3D12_CPU_DESCRIPTOR_HANDLE sampler_cpu_base;
		D3D12_GPU_DESCRIPTOR_HANDLE sampler_gpu_base;

		std::unordered_map<std::string, std::vector<D3D12_CPU_DESCRIPTOR_HANDLE>> texture_semantic_to_binding;
	};

	struct technique_data
	{
		bool has_compute_passes = false;
		std::vector<pass_data> passes;
	};
}
