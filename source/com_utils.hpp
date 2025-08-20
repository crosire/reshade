/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <Unknwn.h>

/// <summary>
/// Gets a pointer from the private data of the specified <paramref name="object"/>.
/// </summary>
template <typename U, typename T>
__forceinline U *get_private_pointer_d3dx(T *object)
{
	U *result = nullptr;
	UINT size = sizeof(result);
	object->GetPrivateData(__uuidof(U), &size, reinterpret_cast<void **>(&result));
	return result;
}
template <typename U, typename T>
__forceinline U *get_private_pointer_d3d9(T *object)
{
	U *result = nullptr;
	DWORD size = sizeof(result);
	object->GetPrivateData(__uuidof(U), reinterpret_cast<void **>(&result), &size);
	return result;
}

/// <summary>
/// Registers a <paramref name="callback"/> with the specified <paramref name="object"/> that is called when the object is being destroyed.
/// </summary>
template <typename T, typename L>
inline void register_destruction_callback_d3dx(T *object, L &&callback)
{
	// Prefer 'ID3DDestructionNotifier' when available (see https://learn.microsoft.com/windows/win32/api/d3dcommon/nn-d3dcommon-id3ddestructionotifier)
	if (com_ptr<ID3DDestructionNotifier> notifier;
		SUCCEEDED(object->QueryInterface(__uuidof(ID3DDestructionNotifier), reinterpret_cast<void **>(&notifier))))
	{
		notifier->RegisterDestructionCallback(
			[](void *data) {
				(*static_cast<L *>(data))();
				delete static_cast<L *>(data);
			}, new L(std::move(callback)), nullptr);
		return;
	}

	class tracker_instance final : public IUnknown
	{
	public:
		// The call to 'SetPrivateDataInterface' below increases the reference count, so set it to zero initially
		tracker_instance(L &&callback) :
			_ref(0), _callback(std::move(callback)) {}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return InterlockedIncrement(&_ref);
		}
		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG ref = InterlockedDecrement(&_ref);
			if (ref == 0)
			{
				_callback();
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
		L const _callback;
	};

	const GUID private_guid = { 0xd6fe4f90, 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

	// Check if a tracker instance was already registered with this object, in which case nothing needs to be done
	if (UINT size = 0;
		SUCCEEDED(object->GetPrivateData(private_guid, &size, nullptr)))
		return;

	IUnknown *const interface_object = new tracker_instance(std::move(callback));
	object->SetPrivateDataInterface(private_guid, interface_object);
}
template <typename T, typename L>
inline void register_destruction_callback_d3d9(T *object, L &&callback, const UINT unique_tag = 0)
{
	class tracker_instance final : public IUnknown
	{
	public:
		// The call to 'SetPrivateData' below increases the reference count, so set it to zero initially
		tracker_instance(L &&callback) :
			_ref(0), _callback(std::move(callback)) {}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return InterlockedIncrement(&_ref);
		}
		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG ref = InterlockedDecrement(&_ref);
			if (ref == 0)
			{
				_callback();
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
		L const _callback;
	};

	const GUID private_guid = { 0xd6fe4f90 + unique_tag, 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

	// Check if a tracker instance was already registered with this object, in which case nothing needs to be done
	if (DWORD size = 0;
		object->GetPrivateData(private_guid, nullptr, &size) != 0x88760866 /* D3DERR_NOTFOUND */)
		return;

	IUnknown *const interface_object = new tracker_instance(std::move(callback));
	object->SetPrivateData(private_guid, interface_object, sizeof(interface_object), 0x1 /* D3DSPD_IUNKNOWN */);
}
