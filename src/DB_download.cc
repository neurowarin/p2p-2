#include "DB_download.h"

DB_download::DB_download()
{
	//open DB
	if(sqlite3_open(global::DATABASE_PATH.c_str(), &sqlite3_DB) != 0){
		logger::debug(LOGGER_P1,"#1 ",sqlite3_errmsg(sqlite3_DB));
	}

	//DB timeout to 1 second
	if(sqlite3_busy_timeout(sqlite3_DB, 1000) != 0){
		logger::debug(LOGGER_P1,"#2 ",sqlite3_errmsg(sqlite3_DB));
	}

	//make download table
	if(sqlite3_exec(sqlite3_DB, "CREATE TABLE IF NOT EXISTS download (hash TEXT, name TEXT, size TEXT, server_IP TEXT, file_ID TEXT);", NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,"#5 ",sqlite3_errmsg(sqlite3_DB));
	}
	if(sqlite3_exec(sqlite3_DB, "CREATE UNIQUE INDEX IF NOT EXISTS hash_index ON download (hash);", NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,"#6 ",sqlite3_errmsg(sqlite3_DB));
	}
}

bool DB_download::get_file_path(const std::string & hash, std::string & path)
{
	boost::mutex::scoped_lock lock(Mutex);

	get_file_path_entry_exits = false;

	//locate the record
	std::ostringstream query;
	query << "SELECT name FROM download WHERE hash = \"" << hash << "\" LIMIT 1;";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), get_file_path_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}

	if(get_file_path_entry_exits){
		path = global::DOWNLOAD_DIRECTORY + get_file_path_file_name;
		return true;
	}
	else{
		logger::debug(LOGGER_P1,"download record not found");
	}
}

void DB_download::get_file_path_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	get_file_path_entry_exits = true;
	get_file_path_file_name.assign(query_response[0]);
}

void DB_download::initial_fill_buff(std::list<download_info> & resumed_download)
{
	boost::mutex::scoped_lock lock(Mutex);

	if(sqlite3_exec(sqlite3_DB, "SELECT * FROM download;", initial_fill_buff_call_back_wrapper, (void *)this, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
	resumed_download.splice(resumed_download.end(), initial_fill_buff_resumed_download);
}

void DB_download::initial_fill_buff_call_back(int & columns_retrieved, char ** query_response, char ** column_name)
{
	namespace fs = boost::filesystem;

	std::string path(query_response[1]);
	path = global::DOWNLOAD_DIRECTORY + path;

	//make sure the file is present
	std::fstream fstream_temp(path.c_str());
	if(fstream_temp.is_open()){ //partial file exists, add to downloadBuffer
		fstream_temp.close();

		fs::path path_boost = fs::system_complete(fs::path(global::DOWNLOAD_DIRECTORY + query_response[1], fs::native));
		uint64_t current_bytes = fs::file_size(path_boost);
		uint64_t latest_request = current_bytes / (global::P_BLOCK_SIZE - 1); //(global::P_BLOCK_SIZE - 1) because control size is 1 byte

		download_info Download_Info(
			true,                                  //resuming
			query_response[0],                     //hash
			query_response[1],                     //name
			strtoull(query_response[2], NULL, 10), //size
			latest_request                         //latest_request
		);

		//get servers
		char delims[] = ",";
		char * result = NULL;
		result = strtok(query_response[3], delims);
		while(result != NULL){
			Download_Info.server_IP.push_back(result);
			result = strtok(NULL, delims);
		}

		//get file_ID's associated with servers
		result = strtok(query_response[4], delims);
		while(result != NULL){
			Download_Info.file_ID.push_back(result);
			result = strtok(NULL, delims);
		}

		initial_fill_buff_resumed_download.push_back(Download_Info);
	}
	else{ //partial file removed, delete entry from database
		std::ostringstream query;
		query << "DELETE FROM download WHERE name = \"" << query_response[1] << "\";";
		if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
			logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
		}
	}
}

bool DB_download::start_download(download_info & info)
{
	boost::mutex::scoped_lock lock(Mutex);

	std::ostringstream query;
	query << "INSERT INTO download (hash, name, size, server_IP, file_ID) VALUES ('" << info.hash << "', '" << info.name << "', " << info.size << ", '";
	for(std::vector<std::string>::iterator iter0 = info.server_IP.begin(); iter0 != info.server_IP.end(); ++iter0){
		if(iter0 + 1 != info.server_IP.end()){
			query << *iter0 << ",";
		}
		else{
			query << *iter0;
		}
	}
	query << "', '";
	for(std::vector<std::string>::iterator iter0 = info.file_ID.begin(); iter0 != info.file_ID.end(); ++iter0){
		if(iter0 + 1 != info.file_ID.end()){
			query << *iter0 << ",";
		}
		else{
			query << *iter0;
		}
	}
	query << "');";

	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) == 0){
		return true;
	}
	else{
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
		return false;
	}
}

void DB_download::terminate_download(const std::string & hash)
{
	boost::mutex::scoped_lock lock(Mutex);

	std::ostringstream query;
	query << "DELETE FROM download WHERE hash = \"" << hash << "\";";
	if(sqlite3_exec(sqlite3_DB, query.str().c_str(), NULL, NULL, NULL) != 0){
		logger::debug(LOGGER_P1,sqlite3_errmsg(sqlite3_DB));
	}
}
