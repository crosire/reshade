/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon_impl.hpp"
#include "reshade_events.hpp"

#if RESHADE_ADDON

namespace reshade
{
	/// <summary>
	/// Loads any add-ons found in the configured search paths.
	/// </summary>
	void load_addons();

	/// <summary>
	/// Unloads any add-ons previously loaded via <see cref="load_addons"/>.
	/// </summary>
	void unload_addons();

	/// <summary>
	/// Enables or disables all loaded add-ons.
	/// </summary>
	void enable_or_disable_addons(bool enabled);

	/// <summary>
	/// Invokes all registered callbacks for the specified <typeparamref name="ev"/>ent.
	/// </summary>
	template <addon_event ev, typename... Args>
	inline std::enable_if_t<addon_event_traits<ev>::type == 1, void> invoke_addon_event(const Args &... args)
	{
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)];
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb) // Generates better code than ranged-based for loop
			reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(args...);
	}
	/// <summary>
	/// Invokes registered callbacks for the specified <typeparamref name="ev"/>ent until a callback reports back as having handled this event by returning <c>true</c>.
	/// </summary>
	template <addon_event ev, typename... Args>
	inline std::enable_if_t<addon_event_traits<ev>::type == 2, bool> invoke_addon_event(const Args &... args)
	{
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)];
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb)
			if (reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(args...))
				return true;
		return false;
	}

	/// <summary>
	/// Invokes registered callbacks for the specified <typeparamref name="ev"/>ent when the specified COM <paramref name="object"/> is destroyed.
	/// </summary>
	template <addon_event ev, typename Handle, typename T>
	inline void invoke_addon_event_on_destruction(api::device *device, T *object)
	{
		static_assert(sizeof(Handle) >= sizeof(object));

		class tracker_instance final : public IUnknown
		{
		public:
			tracker_instance(api::device *device, Handle handle) :
				_ref(0), _device(device), _handle(handle) {}

			ULONG STDMETHODCALLTYPE AddRef() override
			{
				return InterlockedIncrement(&_ref);
			}

			ULONG STDMETHODCALLTYPE Release() override
			{
				const ULONG ref = InterlockedDecrement(&_ref);
				if (ref == 0)
				{
					invoke_addon_event<ev>(_device, _handle);
					delete this;
				}
				return ref;
			}

			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override
			{
				if (riid == __uuidof(IUnknown))
				{
					AddRef();
					*ppvObj = this;
					return S_OK;
				}
				return E_NOINTERFACE;
			}

		private:
			ULONG _ref;
			api::device *const _device;
			Handle const _handle;
		};

		static constexpr GUID private_guid = { 0xd6fe4f90 + static_cast<uint32_t>(ev), 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

		IUnknown *const interface_object = new tracker_instance(device, Handle { reinterpret_cast<uintptr_t>(object) });
		object->SetPrivateDataInterface(private_guid, interface_object);
	}
	template <addon_event ev, typename Handle, typename T>
	inline void invoke_addon_event_on_destruction_d3d9(api::device *device, T *object)
	{
		static_assert(sizeof(Handle) >= sizeof(object));

		class tracker_instance final : public IUnknown
		{
		public:
			tracker_instance(api::device *device, Handle handle) :
				_ref(0), _device(device), _handle(handle) {}

			ULONG STDMETHODCALLTYPE AddRef() override
			{
				return InterlockedIncrement(&_ref);
			}

			ULONG STDMETHODCALLTYPE Release() override
			{
				const ULONG ref = InterlockedDecrement(&_ref);
				if (ref == 0)
				{
					invoke_addon_event<ev>(_device, _handle);
					delete this;
				}
				return ref;
			}

			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **ppvObj) override
			{
				if (riid == __uuidof(IUnknown))
				{
					AddRef();
					*ppvObj = this;
					return S_OK;
				}
				return E_NOINTERFACE;
			}

		private:
			ULONG _ref;
			api::device *const _device;
			Handle const _handle;
		};

		static constexpr GUID private_guid = { 0xd6fe4f90 + static_cast<uint32_t>(ev), 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

		IUnknown *const interface_object = new tracker_instance(device, Handle { reinterpret_cast<uintptr_t>(object) });
		object->SetPrivateData(private_guid, interface_object, sizeof(interface_object), 0x1 /* D3DSPD_IUNKNOWN */);
	}
}

namespace reshade::addon
{
	struct info
	{
		void *handle;
		std::string name;
		std::string description;
	};

	/// <summary>
	/// List of currently loaded add-ons.
	/// </summary>
	extern std::vector<info> loaded_info;

	/// <summary>
	/// List of installed add-on event callbacks.
	/// </summary>
	extern std::vector<void *> event_list[];

#if RESHADE_GUI
	/// <summary>
	/// List of overlays registered by loaded add-ons.
	/// </summary>
	extern std::vector<std::pair<std::string, void(*)(api::effect_runtime *, void *)>> overlay_list;
#endif
}

#endif
