#ifndef H_DATABASE_TABLE_BLACKLIST
#define H_DATABASE_TABLE_BLACKLIST

//boost
#include <boost/thread.hpp>

//custom
#include "atomic_int.hpp"
#include "database_connection.hpp"
#include "global.hpp"

//std
#include <iostream>
#include <sstream>
#include <string>

namespace database{
namespace table{
class blacklist
{
public:
	blacklist();

	/*
	add            - add IP to blacklist
	is_blacklisted - returns true if IP on blacklist
	modified       - returns true if IP added to blacklist since last check
	                 precondition: last_state_seen should be initialized to zero
	*/
	void add(const std::string & IP);
	bool is_blacklisted(const std::string & IP);
	bool modified(int & last_state_seen);
private:
	database::connection DB;

	//starts at zero and increments every time a host is added to blacklist
	static atomic_int<int> blacklist_state;
};

}//end of table namespace
}//end of database namespace
#endif
