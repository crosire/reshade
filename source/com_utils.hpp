/*
 * Copyright (C) 2021 Patrick Mours
 * SPDX-License-Identifier: BSD-3-Clause OR MIT
 */

#pragma once

#include <Unknwn.h>

/// <summary>
/// Gets a pointer from the private data of the specified <paramref name="object"/>.
/// </summary>
template <typename RESULT_TYPE, typename OBJECT_TYPE>
PFORCEINLINE RESULT_TYPE *get_private_pointer_d3dx(OBJECT_TYPE *object)
{
	RESULT_TYPE *result = nullptr;
	UINT size = sizeof(result);
	object->GetPrivateData(__uuidof(RESULT_TYPE), &size, reinterpret_cast<void **>(&result));
	return result;
}
template <typename RESULT_TYPE, typename OBJECT_TYPE>
PFORCEINLINE RESULT_TYPE *get_private_pointer_d3d9(OBJECT_TYPE *object)
{
	RESULT_TYPE *result = nullptr;
	DWORD size = sizeof(result);
	object->GetPrivateData(__uuidof(RESULT_TYPE), reinterpret_cast<void **>(&result), &size);
	return result;
}

/// <summary>
/// Registers a <paramref name="callback"/> with the specified <paramref name="object"/> that is called when the object is being destroyed.
/// </summary>
template <typename OBJECT_TYPE, typename LAMBDA_TYPE>
inline void register_destruction_callback_d3dx(OBJECT_TYPE *object, LAMBDA_TYPE &&callback)
{
	// Prefer 'ID3DDestructionNotifier' when available (see https://learn.microsoft.com/windows/win32/api/d3dcommon/nn-d3dcommon-id3ddestructionotifier)
	if (com_ptr<ID3DDestructionNotifier> notifier;
		SUCCEEDED(object->QueryInterface(__uuidof(ID3DDestructionNotifier), reinterpret_cast<void **>(&notifier))))
	{
		notifier->RegisterDestructionCallback(
			[](void *data) {
				(*static_cast<LAMBDA_TYPE *>(data))();
				delete static_cast<LAMBDA_TYPE *>(data);
			}, new LAMBDA_TYPE(std::move(callback)), nullptr);
		return;
	}

	class tracker_instance final : public IUnknown
	{
	public:
		// The call to 'SetPrivateDataInterface' below increases the reference count, so set it to zero initially
		explicit tracker_instance(LAMBDA_TYPE &&callback) :
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
		LAMBDA_TYPE const _callback;
	};

	const GUID private_guid = { 0xd6fe4f90, 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

	// Check if a tracker instance was already registered with this object, in which case nothing needs to be done
	UINT size = 0;
	if (SUCCEEDED(object->GetPrivateData(private_guid, &size, nullptr)))
		return;

	IUnknown *const interface_object = new tracker_instance(std::move(callback));
	object->SetPrivateDataInterface(private_guid, interface_object);
}
template <typename OBJECT_TYPE, typename LAMBDA_TYPE>
inline void register_destruction_callback_d3d9(OBJECT_TYPE *object, LAMBDA_TYPE &&callback, const UINT unique_tag = 0)
{
	class tracker_instance final : public IUnknown
	{
	public:
		// The call to 'SetPrivateData' below increases the reference count, so set it to zero initially
		explicit tracker_instance(LAMBDA_TYPE &&callback) :
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
		LAMBDA_TYPE const _callback;
	};

	const GUID private_guid = { 0xd6fe4f90 + unique_tag, 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

	// Check if a tracker instance was already registered with this object, in which case nothing needs to be done
	DWORD size = 0;
	if (object->GetPrivateData(private_guid, nullptr, &size) != 0x88760866 /* D3DERR_NOTFOUND */)
		return;

	IUnknown *const interface_object = new tracker_instance(std::move(callback));
	object->SetPrivateData(private_guid, interface_object, sizeof(interface_object), 0x1 /* D3DSPD_IUNKNOWN */);
}

// Interface ID to query the original object from a proxy object
inline constexpr GUID IID_UnwrappedObject = { 0x7f2c9a11, 0x3b4e, 0x4d6a, { 0x81, 0x2f, 0x5e, 0x9c, 0xd3, 0x7a, 0x1b, 0x42 } }; // {7F2C9A11-3B4E-4D6A-812F-5E9CD37A1B42}
