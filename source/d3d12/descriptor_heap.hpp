/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <d3d12.h>
#include "com_ptr.hpp"
#include <vector>
#include <cassert>
#include <shared_mutex>

namespace reshade::d3d12
{
	class descriptor_heap_cpu
	{
		struct heap_info
		{
			com_ptr<ID3D12DescriptorHeap> heap;
			std::vector<bool> state;
			SIZE_T heap_base = 0;
		};

		const UINT pool_size = 1024;

	public:
		descriptor_heap_cpu(ID3D12Device *device, D3D12_DESCRIPTOR_HEAP_TYPE type) :
			_device(device), _type(type)
		{
			_increment_size = device->GetDescriptorHandleIncrementSize(type);
		}

		bool allocate(D3D12_CPU_DESCRIPTOR_HANDLE &handle)
		{
			const std::unique_lock<std::shared_mutex> lock(_mutex);

			for (int attempt = 0; attempt < 2; ++attempt)
			{
				for (heap_info &heap_info : _heap_infos)
				{
					// Find free empty in the heap
					if (const auto it = std::find(heap_info.state.begin(), heap_info.state.end(), false);
						it != heap_info.state.end())
					{
						const size_t index = it - heap_info.state.begin();
						heap_info.state[index] = true; // Mark this entry as being in use

						handle.ptr = heap_info.heap_base + index * _increment_size;
						return true;
					}
				}

				// No more space available in the existing heaps, so create a new one and try again
				if (!allocate_heap())
					break;
			}

			return false;
		}

		void free(D3D12_CPU_DESCRIPTOR_HANDLE handle)
		{
			const std::unique_lock<std::shared_mutex> lock(_mutex);

			for (heap_info &heap_info : _heap_infos)
			{
				const SIZE_T heap_beg = heap_info.heap_base;
				const SIZE_T heap_end = heap_info.heap_base + pool_size * _increment_size;

				if (handle.ptr >= heap_beg && handle.ptr < heap_end)
				{
					const SIZE_T index = (handle.ptr - heap_beg) / _increment_size;

					// Mark free slot in the descriptor heap
					heap_info.state[index] = false;
					break;
				}
			}
		}

	private:
		bool allocate_heap()
		{
			heap_info &heap_info = _heap_infos.emplace_back();

			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = _type;
			desc.NumDescriptors = pool_size;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
			desc.NodeMask = 0;

			if (FAILED(_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap_info.heap))))
			{
				_heap_infos.pop_back();
				return false;
			}

			heap_info.heap_base = heap_info.heap->GetCPUDescriptorHandleForHeapStart().ptr;
			heap_info.state.resize(pool_size);

			return true;
		}

		ID3D12Device *const _device;
		std::vector<heap_info> _heap_infos;
		SIZE_T _increment_size;
		D3D12_DESCRIPTOR_HEAP_TYPE _type;
		std::shared_mutex _mutex;
	};

	template <D3D12_DESCRIPTOR_HEAP_TYPE type, UINT static_size, UINT transient_size>
	class descriptor_heap_gpu
	{
	public:
		explicit descriptor_heap_gpu(ID3D12Device *device, UINT node_mask = 0)
		{
			// Manage all descriptors in a single heap, to avoid costly descriptor heap switches during rendering
			// The lower portion of the heap is reserved for static bindings, the upper portion for transient bindings (which change frequently and are managed like a ring buffer)
			D3D12_DESCRIPTOR_HEAP_DESC desc;
			desc.Type = type;
			desc.NumDescriptors = static_size + transient_size;
			desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
			desc.NodeMask = node_mask;

			if (FAILED(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&_heap))))
				return;

			_increment_size = device->GetDescriptorHandleIncrementSize(type);
			_static_heap_base = _heap->GetCPUDescriptorHandleForHeapStart().ptr;
			_static_heap_base_gpu = _heap->GetGPUDescriptorHandleForHeapStart().ptr;
			_transient_heap_base = _static_heap_base + static_size * _increment_size;
			_transient_heap_base_gpu = _static_heap_base_gpu + static_size * _increment_size;
		}
		~descriptor_heap_gpu()
		{
			assert(_count_list.empty());
		}

		bool allocate_static(UINT count, D3D12_CPU_DESCRIPTOR_HANDLE &base_handle, D3D12_GPU_DESCRIPTOR_HANDLE &base_handle_gpu)
		{
			if (_heap == nullptr)
				return false;

			const std::unique_lock<std::shared_mutex> lock(_mutex);

			// First try to allocate from the list of freed blocks
			for (auto block = _free_list.begin(); block != _free_list.end(); ++block)
			{
				if (count <= (block->second - block->first) / _increment_size)
				{
					base_handle.ptr = _static_heap_base + static_cast<SIZE_T>(block->first - _static_heap_base_gpu);
					base_handle_gpu.ptr = block->first;

					_count_list.emplace_back(base_handle_gpu.ptr, count);

					// Remove the allocated range from the freed block and optionally remove it from the free list if no space is left afterwards
					block->first += count * _increment_size;
					if (block->first == block->second)
						_free_list.erase(block);

					return true;
				}
			}

			// Otherwise follow a linear allocation schema
			if (_current_static_index + count > static_size)
				return false; // The heap is full

			const SIZE_T offset = _current_static_index * _increment_size;
			base_handle.ptr = _static_heap_base + offset;
			base_handle_gpu.ptr = _static_heap_base_gpu + offset;

			_count_list.emplace_back(base_handle_gpu.ptr, count);

			_current_static_index += count;

			return true;
		}
		bool allocate_transient(UINT count, D3D12_CPU_DESCRIPTOR_HANDLE &base_handle, D3D12_GPU_DESCRIPTOR_HANDLE &base_handle_gpu)
		{
			if (_heap == nullptr)
				return false;

			const std::unique_lock<std::shared_mutex> lock(_mutex);

			SIZE_T index = static_cast<SIZE_T>(_current_transient_tail % transient_size);

			// Allocations need to be contiguous
			if (index + count > transient_size)
				_current_transient_tail += transient_size - index, index = 0;

			const SIZE_T offset = index * _increment_size;
			base_handle.ptr = _transient_heap_base + offset;
			base_handle_gpu.ptr = _transient_heap_base_gpu + offset;

			_current_transient_tail += count;

			return true;
		}

		void free(D3D12_GPU_DESCRIPTOR_HANDLE base_handle_gpu)
		{
			// Ensure this handle falls into the static range of this heap
			if (base_handle_gpu.ptr < _static_heap_base_gpu || base_handle_gpu.ptr >= _transient_heap_base_gpu)
				return;

			UINT count = 1;

			const std::unique_lock<std::shared_mutex> lock(_mutex);

			// Find the number of descriptors allocated for this base handle
			for (auto it = _count_list.begin(); it != _count_list.end(); ++it)
			{
				if (base_handle_gpu.ptr == it->first)
				{
					count = it->second;
					_count_list.erase(it);
					break;
				}
			}

			// First try to append to an existing freed block
			for (auto block = _free_list.begin(); block != _free_list.end(); ++block)
			{
				if (base_handle_gpu.ptr == block->second)
				{
					block->second += count * _increment_size;

					// Try and merge with other blocks that are adjacent
					for (auto block_adj = _free_list.begin(); block_adj != _free_list.end(); ++block_adj)
					{
						if (block_adj->first == block->second)
						{
							block->second = block_adj->second;
							_free_list.erase(block_adj);
							break;
						}
					}
					return;
				}
				if (base_handle_gpu.ptr == (block->first - (count * _increment_size)))
				{
					block->first -= count * _increment_size;

					// Try and merge with other blocks that are adjacent
					for (auto block_adj = _free_list.begin(); block_adj != _free_list.end(); ++block_adj)
					{
						if (block_adj->second == block->first)
						{
							block->first = block_adj->first;
							_free_list.erase(block_adj);
							break;
						}
					}
					return;
				}
			}

			// Otherwise add a new block to the free list
			_free_list.emplace_back(base_handle_gpu.ptr, base_handle_gpu.ptr + count * _increment_size);
		}

		bool contains(D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu) const
		{
			return handle_gpu.ptr >= _static_heap_base_gpu && handle_gpu.ptr < _transient_heap_base_gpu;
		}

		bool convert_handle(D3D12_GPU_DESCRIPTOR_HANDLE handle_gpu, D3D12_CPU_DESCRIPTOR_HANDLE &out_handle_cpu) const
		{
			if (contains(handle_gpu))
			{
				out_handle_cpu.ptr = _static_heap_base + static_cast<SIZE_T>(handle_gpu.ptr - _static_heap_base_gpu);
				return true;
			}
			else
			{
				out_handle_cpu.ptr = 0;
				return false;
			}
		}

		ID3D12DescriptorHeap *get() const { assert(_heap != nullptr); return _heap.get(); }

	private:
		com_ptr<ID3D12DescriptorHeap> _heap;
		SIZE_T _increment_size;
		SIZE_T _static_heap_base;
		UINT64 _static_heap_base_gpu;
		SIZE_T _transient_heap_base;
		UINT64 _transient_heap_base_gpu;
		SIZE_T _current_static_index = 0;
		UINT64 _current_transient_tail = 0;
		std::vector<std::pair<UINT64, UINT64>> _free_list;
		std::vector<std::pair<UINT64, UINT32>> _count_list;
		std::shared_mutex _mutex;
	};
}
