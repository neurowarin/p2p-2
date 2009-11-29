#ifndef H_DATABASE_INIT
#define H_DATABASE_INIT

//custom
#include "database.hpp"
#include "path.hpp"

//include
#include <convert.hpp>
#include <random.hpp>
#include <SHA1.hpp>

//standard
#include <sstream>

namespace database{
class init
{
public:
	//this ctor calls create_all()
	init();

	/*
	create_all:
		Create tables that don't yet exist.
	drop_all:
		Drop all tables if they exist.
	*/
	static void create_all();
	static void drop_all();
};
}//end of namespace database
#endif
