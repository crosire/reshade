/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include "addon.hpp"
#include "reshade_events.hpp"

#if RESHADE_ADDON

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
	extern std::pair<bool, std::vector<void *>> event_list[];

#if RESHADE_GUI
	/// <summary>
	/// List of overlays registered by loaded add-ons.
	/// </summary>
	extern std::vector<std::pair<std::string, void(*)(api::effect_runtime *, void *)>> overlay_list;
#endif
}

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
	/// Checks whether any callbacks were registered for the specified <paramref name="ev"/>ent.
	/// </summary>
	/// <param name="ev"></param>
	/// <returns></returns>
	inline bool has_event_callbacks(addon_event ev)
	{
		return !addon::event_list[static_cast<size_t>(ev)].second.empty();
	}

	/// <summary>
	/// Invokes all registered callbacks for the specified <typeparamref name="ev"/>ent.
	/// </summary>
	template <addon_event ev, typename... Args>
	inline std::enable_if_t<std::is_same_v<typename addon_event_traits<ev>::type, void>, void> invoke_addon_event(Args &&... args)
	{
		if (addon::event_list[static_cast<size_t>(ev)].first)
			return;
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)].second;
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb) // Generates better code than ranged-based for loop
			reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(std::forward<Args>(args)...);
	}
	/// <summary>
	/// Invokes registered callbacks for the specified <typeparamref name="ev"/>ent until a callback reports back as having handled this event by returning <c>true</c>.
	/// </summary>
	template <addon_event ev, typename... Args>
	inline std::enable_if_t<std::is_same_v<typename addon_event_traits<ev>::type, bool>, bool> invoke_addon_event(Args &&... args)
	{
		if (addon::event_list[static_cast<size_t>(ev)].first)
			return false;
		std::vector<void *> &event_list = addon::event_list[static_cast<size_t>(ev)].second;
		for (size_t cb = 0, count = event_list.size(); cb < count; ++cb)
			if (reinterpret_cast<typename reshade::addon_event_traits<ev>::decl>(event_list[cb])(std::forward<Args>(args)...))
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

			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **) override
			{
				return E_NOINTERFACE;
			}

		private:
			ULONG _ref;
			api::device *const _device;
			Handle const _handle;
		};

		static constexpr GUID private_guid = { 0xd6fe4f90 + static_cast<uint32_t>(ev), 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

		// Check if a tracker instance was already registered with this object, in which case nothing needs to be done
		if (UINT size = 0;
			SUCCEEDED(object->GetPrivateData(private_guid, &size, nullptr)))
		{
			assert(size == sizeof(IUnknown *));
			return;
		}

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

			HRESULT STDMETHODCALLTYPE QueryInterface(REFIID, void **) override
			{
				return E_NOINTERFACE;
			}

		private:
			ULONG _ref;
			api::device *const _device;
			Handle const _handle;
		};

		static constexpr GUID private_guid = { 0xd6fe4f90 + static_cast<uint32_t>(ev), 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

		// Check if a tracker instance was already registered with this object, in which case nothing needs to be done
		if (DWORD size = 0;
			object->GetPrivateData(private_guid, nullptr, &size) != 0x88760866 /* D3DERR_NOTFOUND */)
		{
			assert(size == sizeof(IUnknown *));
			return;
		}

		IUnknown *const interface_object = new tracker_instance(device, Handle { reinterpret_cast<uintptr_t>(object) });
		object->SetPrivateData(private_guid, interface_object, sizeof(interface_object), 0x1 /* D3DSPD_IUNKNOWN */);
	}
}

#endif
