#include <BadAccessGuards.h>
#include <vector>


// Very small example of implementation for a vector type with a reduced number of methods.
template<typename T>
class ExampleGuardedVector
{
	std::vector<T> impl;
	BA_GUARD_DECL(BAShadow);
public:
	ExampleGuardedVector(){}
	ExampleGuardedVector(ExampleGuardedVector&& rhs)
	{
		BA_GUARD_WRITE(BAShadow);
		impl = std::move(rhs.impl);
	}
	ExampleGuardedVector(const ExampleGuardedVector& rhs)
	{
		BA_GUARD_WRITE(BAShadow);
		impl = rhs.impl;
	}

	~ExampleGuardedVector()
	{
		BA_GUARD_DESTROY(BAShadow);
	}

	ExampleGuardedVector& operator=(ExampleGuardedVector&& rhs)
	{
		BA_GUARD_WRITE(BAShadow);
		impl = std::move(rhs.impl);
		return *this;
	}

	ExampleGuardedVector& operator=(const ExampleGuardedVector& rhs)
	{
		BA_GUARD_WRITE(BAShadow);
		impl = rhs.impl;
		return *this;
	}

	void push_back(const T& val) {

		BA_GUARD_WRITE(BAShadow);
		impl.push_back(val);
	}

	void push_back(T&& val)
	{
		BA_GUARD_WRITE(BAShadow);
		impl.push_back(std::move(val));
	}

	T* data()
	{
		BA_GUARD_READ(BAShadow);
		return impl.data();
	}
	const T* data() const
	{
		BA_GUARD_READ(BAShadow);
		return impl.data();
	}

	size_t size() const
	{
		BA_GUARD_READ(BAShadow);
		return impl.size();
	}

	size_t capacity() const
	{
		BA_GUARD_READ(BAShadow);
		return impl.capacity();
	}

	void resize(size_t newSize)
	{
		BA_GUARD_WRITE(BAShadow);
		impl.resize(newSize);
	}

	void reserve(size_t newCapacity)
	{
		BA_GUARD_WRITE(BAShadow);
		impl.reserve(newCapacity);
	}

	void shrink_to_fit()
	{
		BA_GUARD_WRITE(BAShadow);
		impl.shrink_to_fit();
	}

	void clear() noexcept
	{
		BA_GUARD_WRITE(BAShadow);
		impl.clear();
	}

	T& operator[](size_t index) noexcept
	{
		// We can't know whether it is used as read only or wrote to, accept the limitation and err on the conservative size
		BA_GUARD_READ(BAShadow);
		return impl[index];
	}
	
	const T& operator[](size_t index) const noexcept
	{
		BA_GUARD_READ(BAShadow);
		return impl[index];
	}

	// Note our iterators return pointers directly but could return objects.
	// While creating the iterators should be guarded, you probably don't want to use 
	// the guards for each and every access (iterator dereference) for performance reasons.
	T* begin() noexcept
	{
		// We can't know whether it is used as read only or wrote to, accept the limitation and err on the conservative size
		BA_GUARD_READ(BAShadow);
		return impl.data();
	}
	const T* begin() const noexcept
	{
		BA_GUARD_READ(BAShadow);
		return impl.data();
	}
	T* end() noexcept
	{
		// We can't know whether it is used as read only or wrote to, accept the limitation and err on the conservative size
		BA_GUARD_READ(BAShadow);
		return impl.data() + impl.size();
	}
	const T* end() const noexcept
	{
		BA_GUARD_READ(BAShadow);
		return impl.data() + impl.size();
	}
};
