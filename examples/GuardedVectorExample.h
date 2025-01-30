#include <BadAccessGuards.h>
#include <vector>

// Very small example of implementation for a vector type with a reduced number of methods.
// This is NOT the inteded way to use the library, ideally you would add the guards to the implementation of the container itself!
// Here we are paying the cost of passing things around, especially in Debug builds.
template<typename T>
class ExampleGuardedVector : public std::vector<T>
{
	using super = std::vector<T>;
	BA_GUARD_DECL(BAShadow);
public:
	ExampleGuardedVector(){}
	ExampleGuardedVector(const ExampleGuardedVector& rhs) : super(rhs) { }
	ExampleGuardedVector(ExampleGuardedVector&& rhs) : super(std::move(rhs)) { }

	~ExampleGuardedVector()
	{
		BA_GUARD_DESTROY(BAShadow);
	}

	ExampleGuardedVector& operator=(ExampleGuardedVector&& rhs)
	{
		BA_GUARD_WRITE(BAShadow);
		super::operator=(std::move(rhs));
		return *this;
	}
	
	ExampleGuardedVector& operator=(const ExampleGuardedVector& rhs)
	{
		BA_GUARD_WRITE(BAShadow);
		super::operator=(rhs);
		return *this;
	}

	void push_back(const T& val) {

		BA_GUARD_WRITE(BAShadow);
		super::push_back(val);
	}


	void push_back_noguard(const T& val) {
		super::push_back(val);
	}

	void push_back(T&& val)
	{
		BA_GUARD_WRITE(BAShadow);
		super::push_back(std::move(val));
	}

	void push_back_noguard(T&& val)
	{
		super::push_back(std::move(val));
	}

	T* data()
	{
		BA_GUARD_READ(BAShadow);
		return super::data();
	}
	const T* data() const
	{
		BA_GUARD_READ(BAShadow);
		return super::data();
	}

	typename super::size_type size() const
	{
		BA_GUARD_READ(BAShadow);
		return super::size();
	}

	typename super::size_type capacity() const
	{
		BA_GUARD_READ(BAShadow);
		return super::capacity();
	}

	void resize(typename super::size_type newSize)
	{
		BA_GUARD_WRITE(BAShadow);
		super::resize(newSize);
	}

	void reserve(typename super::size_type newCapacity)
	{
		BA_GUARD_WRITE(BAShadow);
		super::reserve(newCapacity);
	}

	void shrink_to_fit()
	{
		BA_GUARD_WRITE(BAShadow);
		super::shrink_to_fit();
	}

	void clear() noexcept
	{
		BA_GUARD_WRITE(BAShadow);
		super::clear();
	}

	T& operator[](const typename super::size_type index) noexcept
	{
		// We can't know whether it is used as read only or wrote to, accept the limitation and err on the conservative size
		BA_GUARD_READ(BAShadow);
		return super::operator[](index);
	}
	
	const T& operator[](const typename super::size_type  index) const noexcept
	{
		BA_GUARD_READ(BAShadow);
		return super::operator[](index);
	}

	// Note our iterators return pointers directly but could return objects.
	// While creating the iterators should be guarded, you probably don't want to use 
	// the guards for each and every access (iterator dereference) for performance reasons.
	T* begin() noexcept
	{
		// We can't know whether it is used as read only or wrote to, accept the limitation and err on the conservative size
		BA_GUARD_READ(BAShadow);
		return super::data();
	}
	const T* begin() const noexcept
	{
		BA_GUARD_READ(BAShadow);
		return super::data();
	}
	T* end() noexcept
	{
		// We can't know whether it is used as read only or wrote to, accept the limitation and err on the conservative size
		BA_GUARD_READ(BAShadow);
		return super::data() + super::size();
	}
	const T* end() const noexcept
	{
		BA_GUARD_READ(BAShadow);
		return super::data() + super::size();
	}
};
