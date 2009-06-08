//THREADSAFE
/*
Accessing the object within this class using the -> opterator will be locked:
ex: locking_shared_ptr<my_class> x(new my_class);
    x->func();

When using the * dereference operator an extra * will be needed. The returned
proxy object must be "dereferenced":
ex: locking_shared_ptr<int> x(new int);
    **x = 0;
*/

#ifndef H_LOCKING_SHARED_PTR
#define H_LOCKING_SHARED_PTR

//boost
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

template <typename T>
class locking_shared_ptr
{
public:
	//creates empty locking_share_ptr
	locking_shared_ptr():
		pointee_mutex(new boost::recursive_mutex)
	{}

	//this ctor takes a pointer to be wrapped.
	explicit locking_shared_ptr(T * pointee_in):
		pointee(pointee_in),
		pointee_mutex(new boost::recursive_mutex)
	{}

	/*
	Default copy ctor used. The boost::shared_ptr library takes care of
	reference counting and is guaranteed to be thread safe (see boost docs).
	*/
	class proxy
	{
	public:
		proxy(
			typename boost::shared_ptr<T> pointee_in,
			boost::shared_ptr<boost::recursive_mutex> pointee_mutex_in
		):
			pointee(pointee_in),
			pointee_mutex(pointee_mutex_in)
		{
			pointee_mutex->lock();
		}

		~proxy()
		{
			pointee_mutex->unlock();
		}

		T * operator -> () const { return pointee.get(); }
		T & operator * () const { return *pointee; }

	private:
		typename boost::shared_ptr<T> pointee;
		boost::shared_ptr<boost::recursive_mutex> pointee_mutex;
	};

	proxy operator -> () const { return proxy(pointee, pointee_mutex); }
	proxy operator * () const { return proxy(pointee, pointee_mutex); }

	//allows exclusive access for multiple expressions
	void lock() const { pointee_mutex->lock(); }
	void unlock() const { pointee_mutex->unlock(); }

	//get wrapped pointer, does not do locking
	T * get() const
	{
		return pointee.get();
	}

	//assignment
	locking_shared_ptr<T> & operator = (const locking_shared_ptr & rval)
	{
		if(this == &rval){
			return *this;
		}else{
			/*
			Lock both objects and copy. Both locking_shared_ptr's will have a
			pointer to the same mutex.
			*/
			boost::recursive_mutex::scoped_lock(*pointee_mutex);
			boost::recursive_mutex::scoped_lock(*rval.pointee_mutex);
			pointee = rval.pointee;
			pointee_mutex = rval.pointee_mutex;
			return *this;
		}
	}

	//comparison operators
	bool operator == (const locking_shared_ptr<T> & rval)
	{
		boost::recursive_mutex::scoped_lock(*pointee_mutex);
		boost::recursive_mutex::scoped_lock(*rval.pointee_mutex);
		return pointee == rval.pointee;
	}
	bool operator != (const locking_shared_ptr<T> & rval)
	{
		boost::recursive_mutex::scoped_lock(*pointee_mutex);
		boost::recursive_mutex::scoped_lock(*rval.pointee_mutex);
		return pointee != rval.pointee;
	}
	bool operator < (const locking_shared_ptr<T> & rval) const
	{
		boost::recursive_mutex::scoped_lock(*pointee_mutex);
		boost::recursive_mutex::scoped_lock(*rval.pointee_mutex);
		return pointee < rval.pointee;
	}

private:
	typename boost::shared_ptr<T> pointee;
	boost::shared_ptr<boost::recursive_mutex> pointee_mutex;
};
#endif
