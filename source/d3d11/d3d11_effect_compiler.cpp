/**
 * Copyright (C) 2014 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#include "d3d11_runtime.hpp"
#include "d3d11_effect_compiler.hpp"
#include <assert.h>
#include <iomanip>
#include <fstream>
#include <algorithm>
#include <d3dcompiler.h>
#include <spirv_hlsl.hpp>

namespace reshade::d3d11
{
	using namespace reshadefx;

	static inline size_t roundto16(size_t size)
	{
		return (size + 15) & ~15;
	}

	static D3D11_BLEND literal_to_blend_func(unsigned int value)
	{
		switch (value)
		{
		case 0:
			return D3D11_BLEND_ZERO;
		default:
		case 1:
			return D3D11_BLEND_ONE;
		case 2:
			return D3D11_BLEND_SRC_COLOR;
		case 4:
			return D3D11_BLEND_INV_SRC_COLOR;
		case 3:
			return D3D11_BLEND_SRC_ALPHA;
		case 5:
			return D3D11_BLEND_INV_SRC_ALPHA;
		case 6:
			return D3D11_BLEND_DEST_ALPHA;
		case 7:
			return D3D11_BLEND_INV_DEST_ALPHA;
		case 8:
			return D3D11_BLEND_DEST_COLOR;
		case 9:
			return D3D11_BLEND_INV_DEST_COLOR;
		}
	}
	static D3D11_STENCIL_OP literal_to_stencil_op(unsigned int value)
	{
		switch (value)
		{
		default:
		case 1:
			return D3D11_STENCIL_OP_KEEP;
		case 0:
			return D3D11_STENCIL_OP_ZERO;
		case 3:
			return D3D11_STENCIL_OP_REPLACE;
		case 4:
			return D3D11_STENCIL_OP_INCR_SAT;
		case 5:
			return D3D11_STENCIL_OP_DECR_SAT;
		case 6:
			return D3D11_STENCIL_OP_INVERT;
		case 7:
			return D3D11_STENCIL_OP_INCR;
		case 8:
			return D3D11_STENCIL_OP_DECR;
		}
	}
	static DXGI_FORMAT literal_to_format(texture_format value)
	{
		switch (value)
		{
		case texture_format::r8:
			return DXGI_FORMAT_R8_UNORM;
		case texture_format::r16f:
			return DXGI_FORMAT_R16_FLOAT;
		case texture_format::r32f:
			return DXGI_FORMAT_R32_FLOAT;
		case texture_format::rg8:
			return DXGI_FORMAT_R8G8_UNORM;
		case texture_format::rg16:
			return DXGI_FORMAT_R16G16_UNORM;
		case texture_format::rg16f:
			return DXGI_FORMAT_R16G16_FLOAT;
		case texture_format::rg32f:
			return DXGI_FORMAT_R32G32_FLOAT;
		case texture_format::rgba8:
			return DXGI_FORMAT_R8G8B8A8_TYPELESS;
		case texture_format::rgba16:
			return DXGI_FORMAT_R16G16B16A16_UNORM;
		case texture_format::rgba16f:
			return DXGI_FORMAT_R16G16B16A16_FLOAT;
		case texture_format::rgba32f:
			return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case texture_format::dxt1:
			return DXGI_FORMAT_BC1_TYPELESS;
		case texture_format::dxt3:
			return DXGI_FORMAT_BC2_TYPELESS;
		case texture_format::dxt5:
			return DXGI_FORMAT_BC3_TYPELESS;
		case texture_format::latc1:
			return DXGI_FORMAT_BC4_UNORM;
		case texture_format::latc2:
			return DXGI_FORMAT_BC5_UNORM;
		}

		return DXGI_FORMAT_UNKNOWN;
	}

	DXGI_FORMAT make_format_srgb(DXGI_FORMAT format)
	{
		switch (format)
		{
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM:
				return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM:
				return DXGI_FORMAT_BC1_UNORM_SRGB;
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM:
				return DXGI_FORMAT_BC2_UNORM_SRGB;
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM:
				return DXGI_FORMAT_BC3_UNORM_SRGB;
			default:
				return format;
		}
	}
	DXGI_FORMAT make_format_normal(DXGI_FORMAT format)
	{
		switch (format)
		{
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				return DXGI_FORMAT_R8G8B8A8_UNORM;
			case DXGI_FORMAT_BC1_TYPELESS:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
				return DXGI_FORMAT_BC1_UNORM;
			case DXGI_FORMAT_BC2_TYPELESS:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
				return DXGI_FORMAT_BC2_UNORM;
			case DXGI_FORMAT_BC3_TYPELESS:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
				return DXGI_FORMAT_BC3_UNORM;
			default:
				return format;
		}
	}
	DXGI_FORMAT make_format_typeless(DXGI_FORMAT format)
	{
		switch (format)
		{
			case DXGI_FORMAT_R8G8B8A8_UNORM:
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
				return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case DXGI_FORMAT_BC1_UNORM:
			case DXGI_FORMAT_BC1_UNORM_SRGB:
				return DXGI_FORMAT_BC1_TYPELESS;
			case DXGI_FORMAT_BC2_UNORM:
			case DXGI_FORMAT_BC2_UNORM_SRGB:
				return DXGI_FORMAT_BC2_TYPELESS;
			case DXGI_FORMAT_BC3_UNORM:
			case DXGI_FORMAT_BC3_UNORM_SRGB:
				return DXGI_FORMAT_BC3_TYPELESS;
			default:
				return format;
		}
	}

	d3d11_effect_compiler::d3d11_effect_compiler(d3d11_runtime *runtime, const reshadefx::spirv_module &module, std::string &errors, bool) :
		_runtime(runtime),
		_module(&module),
		_errors(errors)
	{
	}

	bool d3d11_effect_compiler::run()
	{
		_d3dcompiler_module = LoadLibraryW(L"d3dcompiler_47.dll");

		if (_d3dcompiler_module == nullptr)
		{
			_d3dcompiler_module = LoadLibraryW(L"d3dcompiler_43.dll");
		}
		if (_d3dcompiler_module == nullptr)
		{
			_errors += "Unable to load D3DCompiler library. Make sure you have the DirectX end-user runtime (June 2010) installed or a newer version of the library in the application directory.\n";
			return false;
		}

		std::vector<uint32_t> spirv_bin;
		_module->write_module(spirv_bin);

		// TODO try catch
		spirv_cross::CompilerHLSL cross(std::move(spirv_bin));

		const auto resources = cross.get_shader_resources();

		// Parse uniform variables
		_uniform_storage_offset = _runtime->get_uniform_value_storage().size();

		//for (const spirv_cross::Resource &ubo : resources.uniform_buffers)
		//{
		//	const auto &struct_type = cross.get_type(ubo.base_type_id);

		//	_constant_buffer_size += cross.get_declared_struct_size(struct_type);

		//	for (uint32_t i = 0; i < struct_type.member_types.size(); ++i)
		//	{
		//		visit_uniform(cross, ubo.base_type_id, i);
		//	}
		//}

		for (const auto &texture : _module->_textures)
		{
			visit_texture(texture);
		}
		for (const auto &sampler : _module->_samplers)
		{
			visit_sampler(sampler);
		}
		for (const auto &uniform : _module->_uniforms)
		{
			visit_uniform(cross, uniform);
		}

		// Compile all entry points to DX byte code
		for (const spirv_cross::EntryPoint &entry : cross.get_entry_points_and_stages())
		{
			compile_entry_point(cross, entry, 50);
		}

		// Parse technique information
		for (const auto &technique : _module->_techniques)
		{
			visit_technique(technique);
		}

		if (_constant_buffer_size != 0)
		{
			_constant_buffer_size = roundto16(_constant_buffer_size);
			_runtime->get_uniform_value_storage().resize(_uniform_storage_offset + _constant_buffer_size);

			const CD3D11_BUFFER_DESC globals_desc(static_cast<UINT>(_constant_buffer_size), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
			const D3D11_SUBRESOURCE_DATA globals_initial = { _runtime->get_uniform_value_storage().data() + _uniform_storage_offset, static_cast<UINT>(_constant_buffer_size) };

			com_ptr<ID3D11Buffer> constant_buffer;
			_runtime->_device->CreateBuffer(&globals_desc, &globals_initial, &constant_buffer);

			_runtime->_constant_buffers.push_back(std::move(constant_buffer));
		}

		FreeLibrary(_d3dcompiler_module);

		return _success;
	}

	void d3d11_effect_compiler::error(const std::string &message)
	{
		_success = false;

		_errors += "error: " + message + '\n';
	}
	void d3d11_effect_compiler::warning(const std::string &message)
	{
		_errors += "warning: " + message + '\n';
	}

	void d3d11_effect_compiler::visit_texture(const spirv_texture_info &texture_info)
	{
		const auto existing_texture = _runtime->find_texture(texture_info.unique_name);

		if (existing_texture != nullptr)
		{
			if (texture_info.semantic.empty() && (
				existing_texture->width != texture_info.width ||
				existing_texture->height != texture_info.height ||
				existing_texture->levels != texture_info.levels ||
				existing_texture->format != static_cast<texture_format>(texture_info.format)))
				error(existing_texture->effect_filename + " already created a texture with the same name but different dimensions; textures are shared across all effects, so either rename the variable or adjust the dimensions so they match");
			return;
		}

		texture obj;
		obj.unique_name = texture_info.unique_name;

		for (const auto &annotation : texture_info.annotations)
			obj.annotations.insert({ annotation.first, variant(annotation.second) });

		obj.width = texture_info.width;
		obj.height = texture_info.height;
		obj.levels = texture_info.levels;
		obj.format = static_cast<texture_format>(texture_info.format);

		D3D11_TEXTURE2D_DESC texdesc = {};
		texdesc.Width = obj.width;
		texdesc.Height = obj.height;
		texdesc.MipLevels = obj.levels;
		texdesc.ArraySize = 1;
		texdesc.Format = literal_to_format(obj.format);
		texdesc.SampleDesc.Count = 1;
		texdesc.SampleDesc.Quality = 0;
		texdesc.Usage = D3D11_USAGE_DEFAULT;
		texdesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
		texdesc.MiscFlags = D3D11_RESOURCE_MISC_GENERATE_MIPS;

		obj.impl = std::make_unique<d3d11_tex_data>();
		const auto obj_data = obj.impl->as<d3d11_tex_data>();

		if (texture_info.semantic == "COLOR")
		{
			obj.width = _runtime->frame_width();
			obj.height = _runtime->frame_height();
			obj.impl_reference = texture_reference::back_buffer;

			obj_data->srv[0] = _runtime->_backbuffer_texture_srv[0];
			obj_data->srv[1] = _runtime->_backbuffer_texture_srv[1];
		}
		else if (texture_info.semantic == "DEPTH")
		{
			obj.width = _runtime->frame_width();
			obj.height = _runtime->frame_height();
			obj.impl_reference = texture_reference::depth_buffer;

			obj_data->srv[0] = _runtime->_depthstencil_texture_srv;
			obj_data->srv[1] = _runtime->_depthstencil_texture_srv;
		}
		else if (!texture_info.semantic.empty())
		{
			error("invalid semantic");
			return;
		}
		else
		{
			HRESULT hr = _runtime->_device->CreateTexture2D(&texdesc, nullptr, &obj_data->texture);

			if (FAILED(hr))
			{
				error("'ID3D11Device::CreateTexture2D' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC srvdesc = { };
			srvdesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvdesc.Texture2D.MipLevels = texdesc.MipLevels;
			srvdesc.Format = make_format_normal(texdesc.Format);

			hr = _runtime->_device->CreateShaderResourceView(obj_data->texture.get(), &srvdesc, &obj_data->srv[0]);

			if (FAILED(hr))
			{
				error("'ID3D11Device::CreateShaderResourceView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			srvdesc.Format = make_format_srgb(texdesc.Format);

			if (srvdesc.Format != texdesc.Format)
			{
				hr = _runtime->_device->CreateShaderResourceView(obj_data->texture.get(), &srvdesc, &obj_data->srv[1]);

				if (FAILED(hr))
				{
					error("'ID3D11Device::CreateShaderResourceView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
					return;
				}
			}
			else
			{
				obj_data->srv[1] = obj_data->srv[0];
			}
		}

		_runtime->add_texture(std::move(obj));
	}

	void d3d11_effect_compiler::visit_sampler(const spirv_sampler_info &sampler_info)
	{
		const auto existing_texture = _runtime->find_texture(sampler_info.texture_name);

		if (!existing_texture)
			return;

		D3D11_SAMPLER_DESC desc = {};
		desc.Filter = static_cast<D3D11_FILTER>(sampler_info.filter);
		desc.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(sampler_info.address_u);
		desc.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(sampler_info.address_v);
		desc.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(sampler_info.address_w);
		desc.MipLODBias = sampler_info.lod_bias;
		desc.MaxAnisotropy = 1;
		desc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		desc.MinLOD = sampler_info.min_lod;
		desc.MaxLOD = sampler_info.max_lod;

		size_t desc_hash = 2166136261;
		for (size_t i = 0; i < sizeof(desc); ++i)
			desc_hash = (desc_hash * 16777619) ^ reinterpret_cast<const uint8_t *>(&desc)[i];

		auto it = _runtime->_effect_sampler_states.find(desc_hash);

		if (it == _runtime->_effect_sampler_states.end())
		{
			com_ptr<ID3D11SamplerState> sampler;

			HRESULT hr = _runtime->_device->CreateSamplerState(&desc, &sampler);

			if (FAILED(hr))
			{
				error("'ID3D11Device::CreateSamplerState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
				return;
			}

			it = _runtime->_effect_sampler_states.emplace(desc_hash, std::move(sampler)).first;
		}

		_sampler_bindings.resize(std::max(_sampler_bindings.size(), size_t(sampler_info.binding + 1)));
		_texture_bindings.resize(std::max(_texture_bindings.size(), size_t(sampler_info.binding + 1)));

		_texture_bindings[sampler_info.binding] = existing_texture->impl->as<d3d11_tex_data>()->srv[sampler_info.srgb ? 1 : 0];
		_sampler_bindings[sampler_info.binding] = it->second;
	}

	void d3d11_effect_compiler::visit_uniform(const spirv_cross::CompilerHLSL &cross, const spirv_uniform_info &uniform_info)
	{
		const auto &member_type = cross.get_type(uniform_info.type_id);
		const auto &struct_type = cross.get_type(uniform_info.struct_type_id);

		uniform obj;
		obj.name = uniform_info.name;
		obj.rows = member_type.vecsize;
		obj.columns = member_type.columns;
		obj.elements = !member_type.array.empty() && member_type.array_size_literal[0] ? member_type.array[0] : 1;
		obj.storage_size = cross.get_declared_struct_member_size(struct_type, uniform_info.member_index);
		obj.storage_offset = _uniform_storage_offset + cross.type_struct_member_offset(struct_type, uniform_info.member_index);

		for (const auto &annotation : uniform_info.annotations)
			obj.annotations.insert({ annotation.first, variant(annotation.second) });

		switch (member_type.basetype)
		{
		case spirv_cross::SPIRType::Int:
			obj.displaytype = obj.basetype = uniform_datatype::signed_integer;
			break;
		case spirv_cross::SPIRType::UInt:
			obj.displaytype = obj.basetype = uniform_datatype::unsigned_integer;
			break;
		case spirv_cross::SPIRType::Float:
			obj.displaytype = obj.basetype = uniform_datatype::floating_point;
			break;
		}

		_constant_buffer_size = std::max(_constant_buffer_size, obj.storage_offset + obj.storage_size - _uniform_storage_offset);

		auto &uniform_storage = _runtime->get_uniform_value_storage();

		if (obj.storage_offset + obj.storage_size >= uniform_storage.size())
		{
			uniform_storage.resize(uniform_storage.size() + 128);
		}

		if (uniform_info.has_initializer_value)
		{
			memcpy(uniform_storage.data() + obj.storage_offset, uniform_info.initializer_value.as_float, obj.storage_size);
		}
		else
		{
			memset(uniform_storage.data() + obj.storage_offset, 0, obj.storage_size);
		}

		_runtime->add_uniform(std::move(obj));
	}

	void d3d11_effect_compiler::visit_technique(const spirv_technique_info &technique_info)
	{
		technique obj;
		obj.impl = std::make_unique<d3d11_technique_data>();
		obj.name = technique_info.name;

		for (const auto &annotation : technique_info.annotations)
			obj.annotations.insert({ annotation.first, variant(annotation.second) });

		auto obj_data = obj.impl->as<d3d11_technique_data>();

		D3D11_QUERY_DESC query_desc = {};
		query_desc.Query = D3D11_QUERY_TIMESTAMP;
		_runtime->_device->CreateQuery(&query_desc, &obj_data->timestamp_query_beg);
		_runtime->_device->CreateQuery(&query_desc, &obj_data->timestamp_query_end);
		query_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
		_runtime->_device->CreateQuery(&query_desc, &obj_data->timestamp_disjoint);

		if (_constant_buffer_size != 0)
		{
			obj.uniform_storage_index = _runtime->_constant_buffers.size();
			obj.uniform_storage_offset = _uniform_storage_offset;
		}

		obj_data->sampler_states = _sampler_bindings;

		for (const auto &pass_info : technique_info.passes)
		{
			auto &pass = static_cast<d3d11_pass_data &>(*obj.passes.emplace_back(std::make_unique<d3d11_pass_data>()));

			pass.vertex_shader = vs_entry_points[pass_info.vs_entry_point];
			pass.pixel_shader = ps_entry_points[pass_info.ps_entry_point];

			pass.viewport.MaxDepth = 1.0f;

			pass.shader_resources = _texture_bindings;
			pass.clear_render_targets = pass_info.clear_render_targets;

			const int target_index = pass_info.srgb_write_enable ? 1 : 0;
			pass.render_targets[0] = _runtime->_backbuffer_rtv[target_index];
			pass.render_target_resources[0] = _runtime->_backbuffer_texture_srv[target_index];

			for (unsigned int k = 0; k < 8; k++)
			{
				const std::string &render_target = pass_info.render_target_names[k];

				if (render_target.empty())
					continue;

				const auto texture = _runtime->find_texture(render_target);

				if (texture == nullptr)
				{
					error("texture not found");
					return;
				}

				d3d11_tex_data *const texture_impl = texture->impl->as<d3d11_tex_data>();

				D3D11_TEXTURE2D_DESC desc;
				texture_impl->texture->GetDesc(&desc);

				if (pass.viewport.Width != 0 && pass.viewport.Height != 0 && (desc.Width != static_cast<unsigned int>(pass.viewport.Width) || desc.Height != static_cast<unsigned int>(pass.viewport.Height)))
				{
					error("cannot use multiple rendertargets with different sized textures");
					return;
				}
				else
				{
					pass.viewport.Width = static_cast<FLOAT>(desc.Width);
					pass.viewport.Height = static_cast<FLOAT>(desc.Height);
				}

				D3D11_RENDER_TARGET_VIEW_DESC rtvdesc = { };
				rtvdesc.Format = pass_info.srgb_write_enable ? make_format_srgb(desc.Format) : make_format_normal(desc.Format);
				rtvdesc.ViewDimension = desc.SampleDesc.Count > 1 ? D3D11_RTV_DIMENSION_TEXTURE2DMS : D3D11_RTV_DIMENSION_TEXTURE2D;

				if (texture_impl->rtv[target_index] == nullptr)
				{
					HRESULT hr = _runtime->_device->CreateRenderTargetView(texture_impl->texture.get(), &rtvdesc, &texture_impl->rtv[target_index]);

					if (FAILED(hr))
					{
						warning("'ID3D11Device::CreateRenderTargetView' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
					}
				}

				pass.render_targets[k] = texture_impl->rtv[target_index];
				pass.render_target_resources[k] = texture_impl->srv[target_index];
			}

			if (pass.viewport.Width == 0 && pass.viewport.Height == 0)
			{
				pass.viewport.Width = static_cast<FLOAT>(_runtime->frame_width());
				pass.viewport.Height = static_cast<FLOAT>(_runtime->frame_height());
			}

			D3D11_DEPTH_STENCIL_DESC ddesc;
			ddesc.DepthEnable = FALSE;
			ddesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
			ddesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
			ddesc.StencilEnable = pass_info.stencil_enable;
			ddesc.StencilReadMask = pass_info.stencil_read_mask;
			ddesc.StencilWriteMask = pass_info.stencil_write_mask;
			ddesc.FrontFace.StencilFunc = ddesc.BackFace.StencilFunc = static_cast<D3D11_COMPARISON_FUNC>(pass_info.stencil_comparison_func);
			ddesc.FrontFace.StencilPassOp = ddesc.BackFace.StencilPassOp = literal_to_stencil_op(pass_info.stencil_op_pass);
			ddesc.FrontFace.StencilFailOp = ddesc.BackFace.StencilFailOp = literal_to_stencil_op(pass_info.stencil_op_fail);
			ddesc.FrontFace.StencilDepthFailOp = ddesc.BackFace.StencilDepthFailOp = literal_to_stencil_op(pass_info.stencil_op_depth_fail);
			pass.stencil_reference = pass_info.stencil_reference_value;

			HRESULT hr = _runtime->_device->CreateDepthStencilState(&ddesc, &pass.depth_stencil_state);

			if (FAILED(hr))
			{
				warning("'ID3D11Device::CreateDepthStencilState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
			}

			D3D11_BLEND_DESC bdesc;
			bdesc.AlphaToCoverageEnable = FALSE;
			bdesc.IndependentBlendEnable = FALSE;
			bdesc.RenderTarget[0].RenderTargetWriteMask = pass_info.color_write_mask;
			bdesc.RenderTarget[0].BlendEnable = pass_info.blend_enable;
			bdesc.RenderTarget[0].BlendOp = static_cast<D3D11_BLEND_OP>(pass_info.blend_op);
			bdesc.RenderTarget[0].BlendOpAlpha = static_cast<D3D11_BLEND_OP>(pass_info.blend_op_alpha);
			bdesc.RenderTarget[0].SrcBlend = literal_to_blend_func(pass_info.src_blend);
			bdesc.RenderTarget[0].DestBlend = literal_to_blend_func(pass_info.dest_blend);
			bdesc.RenderTarget[0].SrcBlendAlpha = literal_to_blend_func(pass_info.src_blend_alpha);
			bdesc.RenderTarget[0].DestBlendAlpha = literal_to_blend_func(pass_info.dest_blend_alpha);

			hr = _runtime->_device->CreateBlendState(&bdesc, &pass.blend_state);

			if (FAILED(hr))
			{
				warning("'ID3D11Device::CreateBlendState' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
			}

			for (auto &srv : pass.shader_resources)
			{
				if (srv == nullptr)
					continue;

				com_ptr<ID3D11Resource> res1;
				srv->GetResource(&res1);

				for (const auto &rtv : pass.render_targets)
				{
					if (rtv == nullptr)
						continue;

					com_ptr<ID3D11Resource> res2;
					rtv->GetResource(&res2);

					if (res1 == res2)
					{
						srv.reset();
						break;
					}
				}
			}
		}

		_runtime->add_technique(std::move(obj));
	}

	void d3d11_effect_compiler::compile_entry_point(spirv_cross::CompilerHLSL &cross, const spirv_cross::EntryPoint &entry, unsigned int shader_model_version)
	{
		std::string hlsl, target;

		// Set shader model version
		cross.set_hlsl_options({ shader_model_version });

		// Cross compile entry point to HLSL source code
		cross.set_entry_point(entry.name, entry.execution_model);

		try
		{
			hlsl = cross.compile();
		}
		catch (const spirv_cross::CompilerError &e)
		{
			error(e.what());
			return;
		}

		// Figure out the target profile name
		switch (entry.execution_model)
		{
		case spv::ExecutionModelVertex:
			target = "vs_" + std::to_string(shader_model_version / 10) + '_' + std::to_string(shader_model_version % 10);
			break;
		case spv::ExecutionModelFragment:
			target = "ps_" + std::to_string(shader_model_version / 10) + '_' + std::to_string(shader_model_version % 10);
			break;
		}

		// Compile the generated HLSL source code to DX byte code
		com_ptr<ID3DBlob> compiled, errors;

		const auto D3DCompile = reinterpret_cast<pD3DCompile>(GetProcAddress(_d3dcompiler_module, "D3DCompile"));

		HRESULT hr = D3DCompile(hlsl.c_str(), hlsl.size(), nullptr, nullptr, nullptr, "main", target.c_str(), D3DCOMPILE_ENABLE_STRICTNESS, 0, &compiled, &errors);

		if (errors != nullptr)
			_errors.append(static_cast<const char *>(errors->GetBufferPointer()), errors->GetBufferSize() - 1); // Subtracting one to not append the null-terminator as well

		if (FAILED(hr))
		{
			error("internal shader compilation failed");
			return;
		}

		// Create runtime shader objects from the compiled DX byte code
		switch (entry.execution_model)
		{
		case spv::ExecutionModelVertex:
			hr = _runtime->_device->CreateVertexShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &vs_entry_points[entry.name]);
			break;
		case spv::ExecutionModelFragment:
			hr = _runtime->_device->CreatePixelShader(compiled->GetBufferPointer(), compiled->GetBufferSize(), nullptr, &ps_entry_points[entry.name]);
			break;
		}

		if (FAILED(hr))
		{
			error("'CreateShader' failed with error code " + std::to_string(static_cast<unsigned long>(hr)) + "!");
			return;
		}
	}
}
