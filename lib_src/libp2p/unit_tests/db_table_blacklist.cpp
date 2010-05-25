//custom
#include "../db_all.hpp"

int fail(0);

int main()
{
	//setup database and make sure blacklist table clear
	path::set_db_file_name("db_table_blacklist.db");
	path::set_program_dir("");
	db::init::drop_all();
	db::init::create_all();

	/*
	The default blacklist state is 0. The blacklist state tells us if the
	blacklist has been modified. If the blacklist has been modified it tells the
	user of the blacklist that they need to check who's currently connected to
	see if anyone has been blacklisted.
	*/
	int state = 0;

	//the blacklist should not be modified by default
	if(db::table::blacklist::modified(state)){
		LOG; ++fail;
	}

	//after adding an IP the blacklist should be modified
	db::table::blacklist::add("1.1.1.1");
	if(!db::table::blacklist::modified(state)){
		LOG; ++fail;
	}

	//the IP we added should be now blacklisted
	if(!db::table::blacklist::is_blacklisted("1.1.1.1")){
		LOG; ++fail;
	}

	//we didn't add this IP, it shouldn't be blacklisted
	if(db::table::blacklist::is_blacklisted("2.2.2.2")){
		LOG; ++fail;
	}
	return fail;
}
