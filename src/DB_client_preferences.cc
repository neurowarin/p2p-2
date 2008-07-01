#include "DB_client_preferences.h"

//used to check if there is an entry in preference tables
static int check_entry_exists(void * object_ptr, int columns_retrieved, char ** query_response, char ** column_name)
{
	if(strcmp(query_response[0], "0") == 0){
		*((bool *)object_ptr) = false;
	}else{
		*((bool *)object_ptr) = true;
	}
	return 0;
}

DB_client_preferences::DB_client_preferences()
{
	//open DB
	if(sqlite3_open(global::DATABASE_PATH.c_str(), &sqlite3_DB) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}

	//DB timeout to 1 second
	if(sqlite3_busy_timeout(sqlite3_DB, 1000) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}

	if(sqlite3_exec(sqlite3_DB, "CREATE TABLE IF NOT EXISTS client_preferences (download_directory TEXT, speed_limit TEXT, max_connections TEXT)", NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	bool client_preferences_exist;
	if(sqlite3_exec(sqlite3_DB, "SELECT count(1) FROM client_preferences", check_entry_exists, (void *)&client_preferences_exist, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	if(!client_preferences_exist){
		std::stringstream query;
		query << "INSERT INTO client_preferences VALUES('" << global::DOWNLOAD_DIRECTORY << "', '" << global::DOWN_SPEED << "', '" << global::MAX_CONNECTIONS << "')";
		if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
			logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
		}
	}
}

std::string DB_client_preferences::get_download_directory()
{
	boost::mutex::scoped_lock lock(Mutex);
	if(sqlite3_exec(sqlite3_DB, "SELECT download_directory FROM client_preferences", get_download_directory_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	return get_download_directory_download_directory;
}

void DB_client_preferences::get_download_directory_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	get_download_directory_download_directory.assign(query_response[0]);
}

void DB_client_preferences::set_download_directory(const std::string & download_directory)
{
	std::ostringstream query;
	query << "UPDATE client_preferences SET download_directory = '" << download_directory << "'";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
}

unsigned int DB_client_preferences::get_speed_limit_uint()
{
	boost::mutex::scoped_lock lock(Mutex);
	if(sqlite3_exec(sqlite3_DB, "SELECT speed_limit FROM client_preferences", get_speed_limit_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}

	std::stringstream ss(get_speed_limit_speed_limit);
	unsigned int speed_limit;
	ss >> speed_limit;
	return speed_limit;
}

void DB_client_preferences::get_speed_limit_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	get_speed_limit_speed_limit.assign(query_response[0]);
}

void DB_client_preferences::set_speed_limit(const unsigned int & speed_limit)
{
	boost::mutex::scoped_lock lock(Mutex);
	std::ostringstream query;
	query << "UPDATE client_preferences SET speed_limit = '" << speed_limit << "'";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
}

int DB_client_preferences::get_max_connections()
{
	boost::mutex::scoped_lock lock(Mutex);
	if(sqlite3_exec(sqlite3_DB, "SELECT max_connections FROM client_preferences", get_max_connections_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}

	std::stringstream ss(get_max_connections_max_connections);
	int max_connections;
	ss >> max_connections;
	return max_connections;
}

void DB_client_preferences::get_max_connections_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	get_max_connections_max_connections.assign(query_response[0]);
}

void DB_client_preferences::set_max_connections(const unsigned int & max_connections)
{
	boost::mutex::scoped_lock lock(Mutex);
	std::ostringstream query;
	query << "UPDATE client_preferences SET max_connections = '" << max_connections << "'";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
}
