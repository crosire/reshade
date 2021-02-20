/*
 * Copyright (C) 2021 Patrick Mours. All rights reserved.
 * License: https://github.com/crosire/reshade#license
 */

#pragma once

#include <mutex>
#include <unordered_set>
#include <Unknwn.h>

template <typename T, bool d3d9_style = false>
class com_object_list
{
	class tracker_instance : public IUnknown
	{
	public:
		tracker_instance(T *object, com_object_list<T, d3d9_style> *list) :
			_ref(0), _list(list), _object(object) {}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return InterlockedIncrement(&_ref);
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG ref = InterlockedDecrement(&_ref);
			if (ref == 0)
			{
				_list->unregister_object(_object);
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
		com_object_list<T, d3d9_style> *_list;
		T *_object;
	};

	static constexpr GUID private_guid = { 0xd6fe4f90, 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

public:
	~com_object_list()
	{
		// Destroy all lifetime tracker objects, so that there are non referencing this list after it was destroyed
		for (auto it = _objects.begin(); it != _objects.end(); it = _objects.begin())
		{
			T *const object = *it;
			object->SetPrivateData(private_guid, 0, nullptr);
		}
	}

	bool has_object(T *object) const
	{
		const std::lock_guard<std::mutex> lock(_mutex);
		return _objects.find(object) != _objects.end();
	}

	void register_object(T *object)
	{
		assert(object != nullptr);

		const std::lock_guard<std::mutex> lock(_mutex);
		if (_objects.find(object) == _objects.end())
		{
			_objects.insert(object);

			object->SetPrivateDataInterface(private_guid, new tracker_instance(object, this));
		}
	}
	void unregister_object(T *object)
	{
		assert(object != nullptr);

		const std::lock_guard<std::mutex> lock(_mutex);
		_objects.erase(object);
	}

private:
	mutable std::mutex _mutex;
	std::unordered_set<T *> _objects;
};

template <typename T>
class com_object_list<T, true>
{
	class tracker_instance : public IUnknown
	{
	public:
		tracker_instance(T *object, com_object_list<T, true> *list) :
			_ref(0), _list(list), _object(object) {}

		ULONG STDMETHODCALLTYPE AddRef() override
		{
			return InterlockedIncrement(&_ref);
		}

		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG ref = InterlockedDecrement(&_ref);
			if (ref == 0)
			{
				_list->unregister_object(_object);
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
		com_object_list<T, true> *_list;
		T *_object;
	};

	static constexpr GUID private_guid = { 0xd6fe4f90, 0x71b7, 0x473c, { 0xbe, 0x83, 0xea, 0x21, 0x9, 0x7a, 0xa3, 0xeb } };

public:
	~com_object_list()
	{
		for (auto it = _objects.begin(); it != _objects.end(); it = _objects.begin())
		{
			T *const object = *it;
			object->SetPrivateData(private_guid, nullptr, 0, 0);
		}
	}

	bool has_object(T *object) const
	{
		const std::lock_guard<std::mutex> lock(_mutex);
		return _objects.find(object) != _objects.end();
	}

	void register_object(T *object)
	{
		assert(object != nullptr);

		const std::lock_guard<std::mutex> lock(_mutex);
		if (_objects.find(object) == _objects.end())
		{
			_objects.insert(object);

			IUnknown *const interface_object = new tracker_instance(object, this);
			object->SetPrivateData(private_guid, interface_object, sizeof(interface_object), 0x1 /* D3DSPD_IUNKNOWN */);
		}
	}
	void unregister_object(T *object)
	{
		assert(object != nullptr);

		const std::lock_guard<std::mutex> lock(_mutex);
		_objects.erase(object);
	}

private:
	mutable std::mutex _mutex;
	std::unordered_set<T *> _objects;
};
