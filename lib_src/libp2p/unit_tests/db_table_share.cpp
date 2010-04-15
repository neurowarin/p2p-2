//custom
#include "../db_all.hpp"

int fail(0);

int main()
{
	//setup database and make sure share table clear
	path::override_database_name("database_table_share.db");
	path::override_program_directory("");
	db::init::drop_all();
	db::init::create_all();

	//test info
	db::table::share::info SI;
	SI.hash = "ABC";
	SI.path = "/foo/bar";
	SI.file_size = 123;
	SI.last_write_time = 123;
	SI.file_state = db::table::share::downloading;

	//file not yet added, lookups shouldn't work
	if(db::table::share::find(SI.hash)){
		LOGGER(logger::utest); ++fail;
	}

	//add file
	db::table::share::add(SI);

	//make sure lookups work
	if(boost::shared_ptr<db::table::share::info>
		lookup_SI = db::table::share::find(SI.hash))
	{
		if(lookup_SI->hash != SI.hash){
			LOGGER(logger::utest); ++fail;
		}
		if(lookup_SI->path != SI.path){
			LOGGER(logger::utest); ++fail;
		}
		if(lookup_SI->file_size != SI.file_size){
			LOGGER(logger::utest); ++fail;
		}
		if(lookup_SI->last_write_time != SI.last_write_time){
			LOGGER(logger::utest); ++fail;
		}
		if(lookup_SI->file_state != SI.file_state){
			LOGGER(logger::utest); ++fail;
		}
	}else{
		LOGGER(logger::utest); ++fail;
	}

	//remove file
	db::table::share::remove(SI.path);

	//file was deleted, make sure lookups don't work
	if(db::table::share::find(SI.hash)){
		LOGGER(logger::utest); ++fail;
	}
	return fail;
}
