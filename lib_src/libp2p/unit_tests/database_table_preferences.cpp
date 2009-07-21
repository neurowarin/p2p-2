//custom
#include "../database.hpp"

int main()
{
	//setup database, there is no need to clear this table for testing
	database::pool::singleton().unit_test_override("database_table_preferences.db");
	database::init init;

	//test set/get of max_download_rate
	database::table::preferences::set_max_download_rate(123);
	if(database::table::preferences::get_max_download_rate() != 123){
		LOGGER; exit(1);
	}

	//test set/get of max_connections
	database::table::preferences::set_max_connections(123);
	if(database::table::preferences::get_max_connections() != 123){
		LOGGER; exit(1);
	}

	//test set/get of max_upload_rate
	database::table::preferences::set_max_upload_rate(123);
	if(database::table::preferences::get_max_upload_rate() != 123){
		LOGGER; exit(1);
	}
}
