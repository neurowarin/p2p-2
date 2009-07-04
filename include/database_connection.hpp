/*
************************** Open Database: **************************************
 database::connection DB("database");

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
	database::blob Blob("test", "test_blob", rowid);

Write blob:
	const char * write_buff = "ABC";
	DB.blob_write(Blob, write_buff, 4, 0);

Read blob:
	char read_buff[4];
	DB.blob_read(Blob, read_buff, 4, 0);
*/
#ifndef H_DATABASE_CONNECTION
#define H_DATABASE_CONNECTION

//include
#include <atomic_int.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <logger.hpp>
#include <portable_sleep.hpp>
#include <sqlite3.h>

namespace database{
//object needed for reading/writing a blob
class blob
{
public:
	blob(
		const std::string & table_in,
		const std::string & column_in,
		const boost::int64_t rowid_in
	):
		table(table_in),
		column(column_in),
		rowid(rowid_in)
	{}

	const std::string table;
	const std::string column;
	const boost::int64_t rowid; //primary key of row
};

class connection : private boost::noncopyable
{
public:
	connection(
		const std::string & path_in
	):
		path(path_in),
		connected(false)
	{}

	~connection()
	{
		if(connected){
			if(sqlite3_close(DB_handle) != SQLITE_OK){
				LOGGER << "sqlite error: " << sqlite3_errmsg(DB_handle);
			}
		}
	}

	//pre-allocate space for blob so that incremental read/write may happen
	bool blob_allocate(const std::string & query, const int & size)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		sqlite3_stmt * prepared_statement;
		if(sqlite3_prepare_v2(
			DB_handle,
			query.c_str(),
			query.size(),
			&prepared_statement,
			0 //not needed unless multiple statements
		) != SQLITE_OK){
			LOGGER << sqlite3_errmsg(DB_handle);
		}
		if(sqlite3_bind_zeroblob(prepared_statement, 1, size) != SQLITE_OK){
			LOGGER << sqlite3_errmsg(DB_handle);
			return false;
		}
		int code;
		while((code = sqlite3_step(prepared_statement)) != SQLITE_DONE){
			LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle);
		}
		if(sqlite3_finalize(prepared_statement) != SQLITE_OK){
			LOGGER << sqlite3_errmsg(DB_handle);
			return false;
		}
		return true;
	}

	//read data from blob
	bool blob_read(blob & Blob, char * const buff, const int & size, const int & offset)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		sqlite3_blob * blob_handle;
		if(!blob_open(Blob, false, blob_handle)){
			return false;
		}
		if(sqlite3_blob_read(blob_handle, static_cast<void *>(buff), size, offset) != SQLITE_OK){
			LOGGER << sqlite3_errmsg(DB_handle);
			return false;
		}
		return blob_close(blob_handle);
	}

	//write data to blob
	bool blob_write(blob & Blob, const char * const buff, const int & size, const int & offset)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		sqlite3_blob * blob_handle;
		if(!blob_open(Blob, true, blob_handle)){
			return false;
		}
		if(sqlite3_blob_write(blob_handle, static_cast<const void *>(buff), size, offset) != SQLITE_OK){
			LOGGER << sqlite3_errmsg(DB_handle);
			return false;
		}
		return blob_close(blob_handle);
	}

	//query with no call back
	int query(const std::string & query)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		int code;
		while((code = sqlite3_exec(DB_handle, query.c_str(), NULL, NULL, NULL)) != SQLITE_OK){
			if(code == SQLITE_BUSY){
				LOGGER << "sqlite yield on SQLITE_BUSY" << " query: " << query;
				portable_sleep::ms(1);
			}else{
				LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle)
					<< " query: " << query;
				break;
			}
		}
		return code;
	}

	//query with function call back
	int query(const std::string & query, int (*fun_ptr)(int, char **, char **))
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		int code;
		while((code = sqlite3_exec(DB_handle, query.c_str(), fun_call_back_wrapper,
			(void *)fun_ptr, NULL)) != SQLITE_OK)
		{
			if(code == SQLITE_BUSY){
				LOGGER << "sqlite yield on SQLITE_BUSY" << " query: " << query;
				portable_sleep::ms(1);
			}else{
				LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle)
					<< " query: " << query;
				break;
			}
		}
		return code;
	}

	//query with function call back and object
	template <typename T>
	int query(const std::string & query, int (*fun_ptr)(T, int, char **, char **), T t)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		//std::pair<func signature, object>
		std::pair<int (*)(T, int, char **, char **), T*> call_back_info(fun_ptr, &t);
		int code;
		while((code = sqlite3_exec(DB_handle, query.c_str(), fun_with_object_call_back_wrapper<T>,
			static_cast<void *>(&call_back_info), NULL)) != SQLITE_OK)
		{
			if(code == SQLITE_BUSY){
				LOGGER << "sqlite yield on SQLITE_BUSY" << " query: " << query;
				portable_sleep::ms(1);
			}else{
				LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle)
					<< " query: " << query;
				break;
			}
		}
		return code;
	}

	//query with function call back and object by reference
	template <typename T>
	int query(const std::string & query, int (*fun_ptr)(T&, int, char **, char **), T & t)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		//std::pair<func signature, object>
		std::pair<int (*)(T&, int, char **, char **), T*> call_back_info(fun_ptr, &t);
		int code;
		while((code = sqlite3_exec(DB_handle, query.c_str(),
			fun_with_object_reference_call_back_wrapper<T>,
			static_cast<void *>(&call_back_info), NULL)) != SQLITE_OK)
		{
			if(code == SQLITE_BUSY){
				LOGGER << "sqlite yield on SQLITE_BUSY" << " query: " << query;
				portable_sleep::ms(1);
			}else{
				LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle)
					<< " query: " << query;
				break;
			}
		}
		return code;
	}

	//query with member function call back
	template <typename T>
	int query(const std::string & query, T * t, int (T::*memfun_ptr)(int, char **, char **))
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		//std::pair<object with func, func signature>
		std::pair<T*, int (T::*)(int, char **, char **)> call_back_info(t, memfun_ptr);
		int code;
		while((code = sqlite3_exec(DB_handle, query.c_str(), memfun_call_back_wrapper<T>,
			static_cast<void *>(&call_back_info), NULL)) != SQLITE_OK)
		{
			if(code == SQLITE_BUSY){
				LOGGER << "sqlite yield on SQLITE_BUSY" << " query: " << query;
				portable_sleep::ms(1);
			}else{
				LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle)
					<< " query: " << query;
				break;
			}
		}
		return code;
	}

	//query with member function call back and object
	template <typename T, typename T_obj>
	int query(const std::string & query, T * t, int (T::*memfun_ptr)(T_obj, int, char **, char **), T_obj T_Obj)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		/*
		std::pair<std::pair<object with func, func signature>, object> >
		A boost::tuple would be nice here but I want to leave this stdlib only.
		*/
		std::pair<std::pair<T*, int (T::*)(T_obj, int, char **, char **)>, T_obj*>
			call_back_info(std::make_pair(t, memfun_ptr), &T_Obj);
		int code;
		while((code = sqlite3_exec(DB_handle, query.c_str(),
			memfun_with_object_call_back_wrapper<T, T_obj>,
			static_cast<void *>(&call_back_info), NULL)) != SQLITE_OK)
		{
			if(code == SQLITE_BUSY){
				LOGGER << "sqlite yield on SQLITE_BUSY" << " query: " << query;
				portable_sleep::ms(1);
			}else{
				LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle)
					<< " query: " << query;
				break;
			}
		}
		return code;
	}

	//query with member function call back and object by reference
	template <typename T, typename T_obj>
	int query(const std::string & query, T * t, int (T::*memfun_ptr)(T_obj&, int, char **, char **), T_obj & T_Obj)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		connect();
		/*
		std::pair<std::pair<object with func, func signature>, object> >
		A boost::tuple would be nice here but I want to leave this stdlib only.
		*/
		std::pair<std::pair<T*, int (T::*)(T_obj&, int, char **, char **)>, T_obj*>
			call_back_info(std::make_pair(t, memfun_ptr), &T_Obj);
		int code;
		while((code = sqlite3_exec(DB_handle, query.c_str(),
			memfun_with_object_reference_call_back_wrapper<T, T_obj>,
			static_cast<void *>(&call_back_info), NULL)) != SQLITE_OK)
		{
			if(code == SQLITE_BUSY){
				LOGGER << "sqlite yield on SQLITE_BUSY" << " query: " << query;
				portable_sleep::ms(1);
			}else{
				LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle)
					<< " query: " << query;
				break;
			}
		}
		return code;
	}

private:
	sqlite3 * DB_handle;
	bool connected;
	std::string path;

	/*
	Recursive_Mutex for all public functions. A recursive mutex is used because a
	thread might want to do a query inside a call back.
	*/
	boost::recursive_mutex Recursive_Mutex;

	/*
	The connection object is lazy. It only connects to the database when a query
	is done. This function connects to the database.
	Precondition: Recursive_Mutex must be locked.
	*/
	void connect()
	{
		if(!connected){
			if(sqlite3_open_v2(path.c_str(), &DB_handle,
				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
				0) != SQLITE_OK)
			{
				LOGGER << "sqlite error: " << sqlite3_errmsg(DB_handle);
				exit(1);
			}

			//timeout in ms, this should not be zero with concurrency
			if(sqlite3_busy_timeout(DB_handle, 4000) != SQLITE_OK){
				LOGGER << "sqlite error: " << sqlite3_errmsg(DB_handle);
				exit(1);
			}

			//move free pages to EOF and truncate at every commit
			int code;
			while((code = sqlite3_exec(DB_handle, "PRAGMA auto_vacuum = full;" ,
				NULL, NULL, NULL)) != SQLITE_OK)
			{
				if(code == SQLITE_BUSY){
					LOGGER << "sqlite yield on SQLITE_BUSY";
					portable_sleep::ms(1);
				}else{
					LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle);
					exit(1);
				}
			}
			connected = true;
			path.clear(); //save space
		}
	}

	//call backs for query functions
	static int fun_call_back_wrapper(void * obj_ptr, int columns, char ** response,
		char ** column_name)
	{
		int (*fun_ptr)(int, char **, char **) = (int (*)(int, char **, char **))obj_ptr;
		return fun_ptr(columns, response, column_name);
	}
	template <typename T>
	static int fun_with_object_call_back_wrapper(void * obj_ptr, int columns,
		char ** response, char ** column_name)
	{
		std::pair<int (*)(T, int, char **, char **), T*> * call_back_info;
		call_back_info = (std::pair<int (*)(T, int, char **, char **), T*> *)obj_ptr;
		int (*fun_ptr)(T, int, char **, char **) = (int (*)(T, int, char **, char **))call_back_info->first;
		return fun_ptr(*(call_back_info->second), columns, response, column_name);
	}

	template <typename T>
	static int fun_with_object_reference_call_back_wrapper(void * obj_ptr, int columns,
		char ** response, char ** column_name)
	{
		std::pair<int (*)(T&, int, char **, char **), T*> * call_back_info;
		call_back_info = (std::pair<int (*)(T&, int, char **, char **), T*> *)obj_ptr;
		int (*fun_ptr)(T&, int, char **, char **) = (int (*)(T&, int, char **, char **))call_back_info->first;
		return fun_ptr(*(call_back_info->second), columns, response, column_name);
	}

	template <typename T>
	static int memfun_call_back_wrapper(void * obj_ptr, int columns, char ** response,
		char ** column_name)
	{
		std::pair<T*, int (T::*)(int, char **, char **)> * call_back_info;
		call_back_info = (std::pair<T*, int (T::*)(int, char **, char **)> *)obj_ptr;
		return ((*call_back_info->first).*(call_back_info->second))(columns, response, column_name);
	}

	template <typename T, typename T_obj>
	static int memfun_with_object_call_back_wrapper(void * obj_ptr, int columns,
		char ** response, char ** column_name)
	{
		std::pair<std::pair<T*, int (T::*)(T_obj, int, char **, char **)>, T_obj*> * call_back_info;
		call_back_info = (std::pair<std::pair<T*, int (T::*)(T_obj, int, char **, char **)>, T_obj*> *)obj_ptr;
		return ((*call_back_info->first.first).*(call_back_info->first.second))
			(*(call_back_info->second), columns, response, column_name);
	}

	template <typename T, typename T_obj>
	static int memfun_with_object_reference_call_back_wrapper(void * obj_ptr,
		int columns, char ** response, char ** column_name)
	{
		std::pair<std::pair<T*, int (T::*)(T_obj&, int, char **, char **)>, T_obj*> * call_back_info;
		call_back_info = (std::pair<std::pair<T*, int (T::*)(T_obj&, int, char **, char **)>, T_obj*> *)obj_ptr;
		return ((*call_back_info->first.first).*(call_back_info->first.second))
			(*(call_back_info->second), columns, response, column_name);
	}

	/*
	Close a blob pointed to by blob handle.
	Precondition: Recursive_Mutex must be locked.
	*/
	bool blob_close(sqlite3_blob * blob_handle)
	{
		if(sqlite3_blob_close(blob_handle) != SQLITE_OK){
			LOGGER << "sqlite error: " << sqlite3_errmsg(DB_handle);
			return false;
		}
		return true;
	}

	/*
	Opens a blob with the info contained in Blob.
	Precondition: Recursive_Mutex must be locked.
	*/
	bool blob_open(blob & Blob, const bool & writeable, sqlite3_blob *& blob_handle)
	{
		assert(!Blob.column.empty());
		assert(!Blob.table.empty());
		assert(Blob.rowid > 0);
		int code;
		while((code = sqlite3_blob_open(
			DB_handle,
			"main",              //symbolic DB name ("main" is default)
			Blob.table.c_str(),
			Blob.column.c_str(),
			Blob.rowid,          //row ID (primary key) of row with blob
			(int)writeable,      //0 = read only, non-zero = read/write
			&blob_handle
		)) != SQLITE_OK){
			if(code == SQLITE_BUSY){
				LOGGER << "sqlite yield on SQLITE_BUSY";
				portable_sleep::ms(1);
			}else{
				LOGGER << "sqlite error " << code << ": " << sqlite3_errmsg(DB_handle);
				return false;
			}
		}
		return true;
	}
};
}//end of database namespace
#endif
