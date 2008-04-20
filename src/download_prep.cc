//std
#include <fstream>
#include <typeinfo>

#include "download_prep.h"

download_prep::download_prep()
{

}

void download_prep::init(volatile int * download_complete_in)
{
	download_complete = download_complete_in;
}

bool download_prep::start_file(download_info & info, download *& Download, std::list<download_conn *> & servers)
{
	//make sure file isn't already downloading
	if(!info.resumed){
		if(!DB_Access.download_start_download(info)){
			return false;
		}
	}

	//get file path, stop if file not found
	std::string file_path;
	if(!DB_Access.download_get_file_path(info.hash, file_path)){
		return false;
	}

	//create an empty file for this download, if a file doesn't already exist
	std::fstream fin(file_path.c_str(), std::ios::in);
	if(fin.is_open()){
		fin.close();
	}else{
		std::fstream fout(file_path.c_str(), std::ios::out);
		fout.close();
	}

	//(global::P_BLS_SIZE - 1) because control size is 1 byte
	uint64_t last_block = info.size/(global::P_BLOCK_SIZE - 1);
	uint64_t last_block_size = info.size % (global::P_BLOCK_SIZE - 1) + 1;

	Download = new download_file(
		info.hash,
		info.name,
		file_path,
		info.size,
		info.latest_request,
		last_block,
		last_block_size,
		download_complete
	);

	assert(info.server_IP.size() == info.file_ID.size());

	for(int x = 0; x < info.server_IP.size(); ++x){
		servers.push_back(new download_file_conn(Download, info.server_IP[x], atoi(info.file_ID[x].c_str())));
	}

	return true;
}

bool download_prep::stop(download * Download_stop, download *& Download_start, std::list<download_conn *> & servers)
{
	if(typeid(*Download_stop) == typeid(download_hash_tree)){
//DEBUG, need to start a download_file here

		return true;
	}else if(typeid(*Download_stop) == typeid(download_file)){
		DB_Access.download_terminate_download(Download_stop->hash());
		delete Download_stop;
		return false;
	}
}
