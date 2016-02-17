#pragma once

#include <assert.h>

template <typename T>
class com_ptr
{
public:
	com_ptr() : _object(nullptr) { }
	com_ptr(T *object) : _object(nullptr)
	{
		reset(object);
	}
	com_ptr(const com_ptr<T> &copy) : _object(nullptr)
	{
		reset(copy._object);
	}
	~com_ptr()
	{
		reset();
	}

	unsigned long ref_count() const
	{
		return _object->AddRef(), _object->Release();
	}

	inline T *get() const
	{
		return _object;
	}
	T &operator*() const
	{
		assert(_object != nullptr);

		return *_object;
	}
	T *operator->() const
	{
		assert(_object != nullptr);

		return _object;
	}
	T **operator&() throw()
	{
		assert(_object == nullptr);

		return &_object;
	}

	void reset(T *object = nullptr)
	{
		if (_object != nullptr)
		{
			_object->Release();
		}

		_object = object;

		if (_object != nullptr)
		{
			_object->AddRef();
		}
	}

	bool operator==(T *other) const
	{
		return _object == other;
	}
	bool operator==(const com_ptr<T> &other) const
	{
		return _object == other._object;
	}
	friend bool operator==(T *left, const com_ptr<T> &right)
	{
		return right.operator==(left);
	}
	bool operator!=(T *other) const
	{
		return _object != other;
	}
	bool operator!=(const com_ptr<T> &other) const
	{
		return _object != other._object;
	}
	friend bool operator!=(T *left, const com_ptr<T> &right)
	{
		return right.operator!=(left);
	}

private:
	T *_object;
};
