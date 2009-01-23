/*
Short:
	This container wraps all the call back and incremental blob reading/writing
	features of SQLite2.

Copying:
	This container may be copied, but when it is the same database connection is
	used for every copy. This container (and copies of this container) are thread
	safe.

Ref Counting:
	This object reference counts. The blob_handle's are closed when they are
	destructed. When all sqlite3_wrapper and blob_handle instantations are
	destructed the database connection is closed.

************************** Open Database: **************************************
 sqlite3_wrapper DB("./database");

************************** Query: **********************************************
No call back:
 DB.query("SELECT * FROM table");

With function call back:
 DB.query("SELECT * FROM table", call_back);               //non-member function
 DB.query("SELECT * FROM table", &Test, &test::call_back); //member function

With function call back with object:
 test Test;
 std::string str = "NARF!";
 DB.query("SELECT * FROM table", call_back_with_object, &str);
 DB.query("SELECT * FROM table", &Test, &test::call_back_with_object, &str);

************************** Example Call Back Functions: ************************
Regular call back:
	int call_back(int columns, char ** response, char ** column_name);

Call back with object (can be value or reference):
	int call_back(std::string str, int columns, char ** response, char ** column_name);
	int call_back(std::string & str, int columns, char ** response, char ** column_name);

************************** Blobs: **********************************************
Create table with blob field:
	DB.query("CREATE TABLE test(test_blob BLOB)");

Allocate blob of a specified size:
	boost::int64_t rowid = DB.blob_allocate("INSERT INTO test(test_blob) VALUES(?)", 1);

Resizing blob is not possible, but you can replace a blob:
	boost::int64_t rowid = DB.blob_allocate("UPDATE test SET test_blob = ?", 2);

Open a blob (rowid is required, hope you didn't lose it!'):
	sqlite3_wrapper::blob Blob("test", "test_blob", rowid);

Write blob:
	const char * write_buff = "ABC";
	Blob.write(BH, write_buff, 4, 0);

Read blob:
	char read_buff[4];
	Blob.read(BH, read_buff, 4, 0);
*/
#ifndef H_SQLITE3_WRAPPER
#define H_SQLITE3_WRAPPER

//boost
#include <boost/thread/mutex.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/shared_ptr.hpp>

//custom
#include "atomic_int.h"
#include "global.h"

//sqlite
#include <sqlite3.h>

//std
#include <exception>
#include <iostream>

class database
{
public:
	database();
	database(const database & DB);
	~database();

	class blob
	{
		friend class database;
	public:
		blob(
			const std::string & table_in,
			const std::string & column_in,
			const boost::int64_t & rowid_in
		):
			table(table_in),
			column(column_in),
			rowid(rowid_in)
		{}

		const std::string table;
		const std::string column;
		const boost::int64_t rowid;

	private:
		sqlite3_blob * blob_handle;
	};

	//allocate blob of specified size
	boost::int64_t blob_allocate(const std::string & query, const int & size);
	void blob_read(blob & Blob, char * const buff, const int & size, const int & offset);
	void blob_write(blob & Blob, const char * const buff, const int & size, const int & offset);

	//query with no call back
	int query(const std::string & query);

	//query with function call back
	int query(const std::string & query, int (*fun_ptr)(int, char **, char **));

	//query with function call back and object
	template <typename T>
	int query(const std::string & query, int (*fun_ptr)(T, int, char **, char **), T t)
	{
		boost::recursive_mutex::scoped_lock lock(*Mutex);
		//std::pair<func signature, object>
		std::pair<int (*)(T, int, char **, char **), T*> call_back_info(fun_ptr, &t);
		int code;
		if((code = sqlite3_exec(DB_handle, query.c_str(), fun_with_object_call_back_wrapper<T>,
			(void *)&call_back_info, NULL)) != SQLITE_OK)
		{
			LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle) << " query: " << query;
		}
		return code;
	}

	//query with function call back and object by reference
	template <typename T>
	int query(const std::string & query, int (*fun_ptr)(T&, int, char **, char **), T & t)
	{
		boost::recursive_mutex::scoped_lock lock(*Mutex);
		//std::pair<func signature, object>
		std::pair<int (*)(T&, int, char **, char **), T*> call_back_info(fun_ptr, &t);
		int code;
		if((code = sqlite3_exec(DB_handle, query.c_str(), fun_with_object_reference_call_back_wrapper<T>,
			(void *)&call_back_info, NULL)) != SQLITE_OK)
		{
			LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle) << " query: " << query;
		}
		return code;
	}

	//query with member function call back
	template <typename T>
	int query(const std::string & query, T * t, int (T::*memfun_ptr)(int, char **, char **))
	{
		boost::recursive_mutex::scoped_lock lock(*Mutex);
		//std::pair<object with func, func signature>
		std::pair<T*, int (T::*)(int, char **, char **)> call_back_info(t, memfun_ptr);
		int code;
		if((code = sqlite3_exec(DB_handle, query.c_str(), memfun_call_back_wrapper<T>,
			(void *)&call_back_info, NULL)) != SQLITE_OK)
		{
			LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle) << " query: " << query;
		}
		return code;
	}

	//query with member function call back and object
	template <typename T, typename T_obj>
	int query(const std::string & query, T * t, int (T::*memfun_ptr)(T_obj, int, char **, char **), T_obj T_Obj)
	{
		boost::recursive_mutex::scoped_lock lock(*Mutex);
		/*
		std::pair<std::pair<object with func, func signature>, object> >
		A boost::tuple would be nice here but I want to leave this stdlib only.
		*/
		std::pair<std::pair<T*, int (T::*)(T_obj, int, char **, char **)>, T_obj*>
			call_back_info(std::make_pair(t, memfun_ptr), &T_Obj);
		int code;
		if((code = sqlite3_exec(DB_handle, query.c_str(), memfun_with_object_call_back_wrapper<T, T_obj>,
			(void *)&call_back_info, NULL)) != SQLITE_OK)
		{
			LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle) << " query: " << query;
		}
		return code;
	}

	//query with member function call back and object by reference
	template <typename T, typename T_obj>
	int query(const std::string & query, T * t, int (T::*memfun_ptr)(T_obj&, int, char **, char **), T_obj & T_Obj)
	{
		boost::recursive_mutex::scoped_lock lock(*Mutex);
		/*
		std::pair<std::pair<object with func, func signature>, object> >
		A boost::tuple would be nice here but I want to leave this stdlib only.
		*/
		std::pair<std::pair<T*, int (T::*)(T_obj&, int, char **, char **)>, T_obj*>
			call_back_info(std::make_pair(t, memfun_ptr), &T_Obj);
		int code;
		if((code = sqlite3_exec(DB_handle, query.c_str(), memfun_with_object_reference_call_back_wrapper<T, T_obj>,
			(void *)&call_back_info, NULL)) != SQLITE_OK)
		{
			LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle) << " query: " << query;
		}
		return code;
	}

private:
	sqlite3 * DB_handle;
	boost::shared_ptr<atomic_int<int> > ref_cnt;

	/*
	Mutex for all public functions. A recursive mutex is used because a thread
	might want to do a query inside a call back.
	*/
	boost::shared_ptr<boost::recursive_mutex> Mutex;

	//call back wrapper for functions
	static int fun_call_back_wrapper(void * obj_ptr, int columns, char ** response, char ** column_name)
	{
		int (*fun_ptr)(int, char **, char **) = (int (*)(int, char **, char **))obj_ptr;
		return fun_ptr(columns, response, column_name);
	}

	//call back wrapper for functions with an object
	template <typename T>
	static int fun_with_object_call_back_wrapper(void * obj_ptr, int columns, char ** response, char ** column_name)
	{
		std::pair<int (*)(T, int, char **, char **), T*> * call_back_info;
		call_back_info = (std::pair<int (*)(T, int, char **, char **), T*> *)obj_ptr;
		int (*fun_ptr)(T, int, char **, char **) = (int (*)(T, int, char **, char **))call_back_info->first;
		return fun_ptr(*(call_back_info->second), columns, response, column_name);
	}

	//call back wrapper for functions with an object by reference
	template <typename T>
	static int fun_with_object_reference_call_back_wrapper(void * obj_ptr, int columns, char ** response, char ** column_name)
	{
		std::pair<int (*)(T&, int, char **, char **), T*> * call_back_info;
		call_back_info = (std::pair<int (*)(T&, int, char **, char **), T*> *)obj_ptr;
		int (*fun_ptr)(T&, int, char **, char **) = (int (*)(T&, int, char **, char **))call_back_info->first;
		return fun_ptr(*(call_back_info->second), columns, response, column_name);
	}

	//call back wrapper for member functions
	template <typename T>
	static int memfun_call_back_wrapper(void * obj_ptr, int columns, char ** response, char ** column_name)
	{
		std::pair<T*, int (T::*)(int, char **, char **)> * call_back_info;
		call_back_info = (std::pair<T*, int (T::*)(int, char **, char **)> *)obj_ptr;
		return ((*call_back_info->first).*(call_back_info->second))(columns, response, column_name);
	}

	//call back wrapper for member functions with an object
	template <typename T, typename T_obj>
	static int memfun_with_object_call_back_wrapper(void * obj_ptr, int columns, char ** response, char ** column_name)
	{
		std::pair<std::pair<T*, int (T::*)(T_obj, int, char **, char **)>, T_obj*> * call_back_info;
		call_back_info = (std::pair<std::pair<T*, int (T::*)(T_obj, int, char **, char **)>, T_obj*> *)obj_ptr;
		return ((*call_back_info->first.first).*(call_back_info->first.second))
			(*(call_back_info->second), columns, response, column_name);
	}

	//call back wrapper for member functions with an object by reference
	template <typename T, typename T_obj>
	static int memfun_with_object_reference_call_back_wrapper(void * obj_ptr, int columns, char ** response, char ** column_name)
	{
		std::pair<std::pair<T*, int (T::*)(T_obj&, int, char **, char **)>, T_obj*> * call_back_info;
		call_back_info = (std::pair<std::pair<T*, int (T::*)(T_obj&, int, char **, char **)>, T_obj*> *)obj_ptr;
		return ((*call_back_info->first.first).*(call_back_info->first.second))
			(*(call_back_info->second), columns, response, column_name);
	}

	//opens specified blob
	void blob_close(blob & Blob);

	//closes specified blob
	void blob_open(blob & Blob, const bool & writeable);
};
#endif
