//THREADSAFE
#ifndef H_DATABASE_TABLE_HASH
#define H_DATABASE_TABLE_HASH

//boost
#include <boost/thread.hpp>

//custom
#include "path.hpp"
#include "settings.hpp"

//include
#include <database_connection.hpp>

//std
#include <sstream>

namespace database{
namespace table{
class hash
{
public:
	enum state{
		RESERVED,    //0 - reserved, (deleted upon program start)
		DOWNLOADING, //1 - downloading, incomplete tree
		COMPLETE     //2 - complete, hash tree complete and checked
	};

	/*
	delete_tree:
		Delete entry for tree of specified hash and tree_size.
	exists:
		Returns true with specified info exists.
	get_state:
		Returns state of hash tree (see state enum above).
	tree_allocate:
		Allocates space for tree with specified hash and size. The set_state
		function must be called for the hash tree to not be deleted on next
		program start.
	*/
	static void delete_tree(const std::string & hash, const int & tree_size, database::connection & DB);
	static bool exists(const std::string & hash, const int & tree_size, database::connection & DB);
	static bool get_state(const std::string & hash, const int & tree_size, state & State, database::connection & DB);
	static void set_state(const std::string & hash, const int & tree_size, const state & State, database::connection & DB);
	static bool tree_allocate(const std::string & hash, const int & tree_size, database::connection & DB);
	static database::blob tree_open(const std::string & hash, const int & tree_size, database::connection & DB);

private:
	hash(){}
};
}//end of table namespace
}//end of database namespace
#endif
