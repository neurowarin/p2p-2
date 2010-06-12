#ifndef H_DB_INIT
#define H_DB_INIT

//custom
#include "db_all.hpp"
#include "path.hpp"

//include
#include <convert.hpp>
#include <portable.hpp>
#include <SHA1.hpp>

//standard
#include <sstream>

namespace db{
class init
{
public:
	/*
	create_all:
		Create tables that don't yet exist.
	drop_all:
		Drop all tables if they exist.
	*/
	static void create_all();
	static void drop_all();

private:
	init(){}
};
}//end of namespace database
#endif
