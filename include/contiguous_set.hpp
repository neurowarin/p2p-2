/*
This container wraps a std::set. It has functions useful for dealing with a
range of elements that arrive out of order where you need to be able to
determine what elements are contiguous.

This container has trim functions which advance the lowest possible key it
will hold. There are many uses where it is natural to do this such as when
checking hash blocks.

There is also a contiguous_map. Both templates have the same functionality.

Note: objects used within contiguous require a lot of different operators to
be overloaded. If you need to associate something with a key use the
contiguous_map instead.
*/

#ifndef H_CONTIGUOUS_SET
#define H_CONTIGUOUS_SET

//std
#include <cassert>
#include <set>

template <typename T_key>
class contiguous_set
{
public:
	//if this ctor is used set_range() should be called
	contiguous_set()
	{
		set_range(0, 0);
	}

	/*
	start_key: lowest possible element insertable in to range
	end_key:   one past highest possible element insertable in to range
	*/
	contiguous_set(const T_key & start_key_in, const T_key & end_key_in)
	{
		set_range(start_key_in, end_key_in);
	}

	/*
	Clears the contiguous map and sets the range.
	*/
	void set_range(const T_key & start_key_in, const T_key & end_key_in)
	{
		range.clear();
		start_key = start_key_in;
		end_key = end_key_in;
	}

	//normal iteration done with regular std::map iterator
	typedef typename std::set<T_key>::iterator iterator;

	//support erasing with normal iterator
	void erase(const iterator & Iter)
	{
		range.erase(Iter);
	}

	//forward iterator to iterate through contiguous elements
	class contiguous_iterator
	{
		friend class contiguous_set;
	public:
		contiguous_iterator(){}
		const contiguous_iterator & operator = (const contiguous_iterator & rval)
		{
			iter = rval.iter;
			return *this;
		}

		bool operator == (const contiguous_iterator & rval)
		{
			return iter == rval.iter;
		}

		bool operator != (const contiguous_iterator & rval)
		{
			return iter != rval.iter;
		}

		//pre-increment
		contiguous_iterator & operator ++ ()
		{
			++iter;
			return *this;
		}

		//post-increment
		contiguous_iterator operator ++ (int)
		{
			typename std::set<T_key>::iterator iter_tmp = iter;
			++iter;
			return contiguous_iterator(iter_tmp);
		}

		const T_key & operator * ()
		{
			return *iter;
		}

		//inline dereference, iterator acts like pointer
		typename std::set<T_key>::iterator & operator -> ()
		{
			return iter;
		}

	private:
		//ctor for use by contiguous class only
		contiguous_iterator(
			const typename std::set<T_key>::iterator & iter_in
		):
			iter(iter_in)
		{}

		typename std::set<T_key>::iterator iter;
	};

	//support erasing with contiguous iterator
	void erase(const contiguous_iterator & Iter)
	{
		range.erase(Iter.iter);
	}

	//forward iterator to iterate through incontinuous elements
	class incontiguous_iterator
	{
		friend class contiguous_set;
	public:
		incontiguous_iterator(){}

		const incontiguous_iterator & operator = (const incontiguous_iterator & rval)
		{
			iter = rval.iter;
			return *this;
		}

		bool operator == (const incontiguous_iterator & rval)
		{
			return iter == rval.iter;
		}

		bool operator != (const incontiguous_iterator & rval)
		{
			return iter != rval.iter;
		}

		//pre-increment
		const incontiguous_iterator & operator ++ ()
		{
			++iter;
			return *this;
		}

		//post-increment
		incontiguous_iterator operator ++ (int)
		{
			typename std::set<T_key>::iterator iter_tmp = iter;
			++iter;
			return incontiguous_iterator(iter_tmp);
		}

		const T_key & operator * ()
		{
			return *iter;
		}

		//inline dereference, iterator acts like pointer
		typename std::set<T_key>::iterator & operator -> ()
		{
			return iter;
		}

	private:
		//ctor for use by contiguous class only
		incontiguous_iterator(
			const typename std::set<T_key>::iterator & iter_in
		):
			iter(iter_in)
		{}

		typename std::set<T_key>::iterator iter;
	};

	//support erasing with incontiguous iterator
	void erase(const incontiguous_iterator & Iter)
	{
		range.erase(Iter.iter);
	}

	//iterator to iterate through all elements
	typename std::set<T_key>::iterator begin()
	{
		return range.begin();
	}

	//iterator to iterate through all elements
	typename std::set<T_key>::iterator end()
	{
		return range.end();
	}

	//returns iterator to beginning of contiguous range
	contiguous_iterator begin_contiguous()
	{
		if(!range.empty() && start_key == *range.begin()){
			return contiguous_iterator(range.begin());
		}else{
			return contiguous_iterator(range.end());
		}
	}

	//returns iterator to end of contiguous range
	contiguous_iterator end_contiguous()
	{
		T_key HC;
		if(highest_contiguous(HC)){
			typename std::set<T_key>::iterator iter;
			//HC guaranteed to be found since highest_contiguous returned true
			iter = range.upper_bound(HC);
			return contiguous_iterator(iter);
		}else{
			return contiguous_iterator(range.end());
		}
	}

	//returns iterator to beginning of incontiguous range
	incontiguous_iterator begin_incontiguous()
	{
		T_key HC;
		if(highest_contiguous(HC)){
			typename std::set<T_key>::iterator iter;
			//HC guaranteed to be found since highest_contiguous returned true
			iter = range.upper_bound(HC);
			return incontiguous_iterator(iter);
		}else{
			return incontiguous_iterator(range.end());
		}
	}

	//returns iterator to end of incontiguous range
	incontiguous_iterator end_incontiguous()
	{
		return incontiguous_iterator(range.end());
	}

	//erase element with key TK
	void erase(const T_key & TK)
	{
		range.erase(TK);
	}

	//find element with key TK
	typename std::set<T_key>::iterator find(const T_key & TK)
	{
		return range.find(TK);
	}

	//insert element in to range
	void insert(const T_key & TK)
	{
		assert(!(TK < start_key) && TK < end_key);
		range.insert(TK);
	}

	/*
	Sets HC to highest contiguous key starting at start_key.
	example:
		assume start_key = 0
		0 1 2 5 6 9
		return true and set HC to 2
	example:
		assume start_key = 0
		1 2 5 6 9
		return false and don't set HC (notice key 0 is missing)
	*/
	bool highest_contiguous(T_key & HC)
	{
		T_key tmp = start_key;
		typename std::set<T_key>::iterator iter_cur, iter_end;
		iter_cur = range.begin();
		iter_end = range.end();
		while(iter_cur != iter_end){
			if(*iter_cur == tmp){
				++tmp;
			}else{
				break;
			}
			++iter_cur;
		}

		if(tmp == start_key){
			//start key doesn't exist in range, failure
			return false;
		}else{
			//contiguous elements found
			HC = tmp - 1;
			return true;
		}
	}

	//returns the lower boundary of the range (inclusive)
	const T_key & start_range()
	{
		return start_key;
	}

	//returns upper boundary of the range (exclusive)
	const T_key & end_range()
	{
		return end_key;
	}

	/*
	Removes the contiguous part, if there is any. Sets start_key to one past the
	last contigous key if a removal is done.
	*/
	void trim_contiguous()
	{
		T_key HC;
		if(highest_contiguous(HC)){
			start_key = HC + 1;
			typename std::set<T_key>::iterator iter;
			//HC guaranteed to be found since highest_contiguous returned true
			iter = range.upper_bound(HC);
			//erase to one past HC
			range.erase(range.begin(), iter);
		}
	}

	/*
	Removes all elements from start_key up to, but not including num.
	precondition: num must be >= start_key
	*/
	void trim(const T_key & num)
	{
		assert(num >= start_key);
		start_key = num;
		typename std::set<T_key>::iterator iter = range.find(num);
		range.erase(range.begin(), iter);
	}

private:
	T_key start_key; //first key in range
	T_key end_key;   //one past the last key in range

	//stores items that arrive out of order
	std::set<T_key> range;
};
#endif
