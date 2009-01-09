#include "download_factory.h"

download_factory::download_factory()
{

}

download_file * download_factory::start_file(download_hash_tree * DHT, std::list<download_connection> & servers)
{
	std::string file_path;
	if(!DB_Download.lookup_hash(DHT->hash(), file_path)){
		LOGGER << "download " << DHT->download_file_name() << " not found in database";
	}
	download_file * Download = new download_file(
		DHT->hash(),
		DHT->download_file_name(),
		file_path,
		DHT->download_file_size()
	);
	std::vector<std::string> IP;
	DB_Search.get_servers(DHT->hash(), IP);
	for(int x = 0; x < IP.size(); ++x){
		servers.push_back(download_connection(Download, IP[x]));
	}
	return Download;
}

bool download_factory::start_hash(const download_info & info, download *& Download, std::list<download_connection> & servers)
{
	if(DB_Share.lookup_hash(info.hash)){
		//file exists in share, don't redownload it
		LOGGER << "file '" << info.name << "' already exists in share";
		return false;
	}

	if(client_buffer::is_downloading(info.hash)){
		//file already downloading
		LOGGER << "file '" << info.name << "' already downloading";
		return false;
	}

	//add download to download table if it doesn't already exist
	DB_Download.start_download(info);

	/*
	Create empty file for the download_file. This is needed because if the file is
	missing upon program start the download is cancelled.
	*/
	std::fstream fs((global::DOWNLOAD_DIRECTORY+info.name).c_str(), std::ios::in);
	if(!fs.is_open()){
		fs.open((global::DOWNLOAD_DIRECTORY+info.name).c_str(), std::ios::out);
	}

	Download = new download_hash_tree(info.hash, info.size, info.name);
	for(int x = 0; x < info.IP.size(); ++x){
		servers.push_back(download_connection(Download, info.IP[x]));
	}

	return true;
}

bool download_factory::stop(download * Download_Stop, download *& Download_Start, std::list<download_connection> & servers)
{
	if(typeid(*Download_Stop) == typeid(download_hash_tree)){
		if(((download_hash_tree *)Download_Stop)->canceled() == true){
			//user cancelled download, don't trigger download_file start
			DB_Download.terminate_download(Download_Stop->hash());
			delete Download_Stop;
			return false;
		}else{
			//trigger download_file start
			Download_Start = start_file((download_hash_tree *)Download_Stop, servers);
			delete Download_Stop;
			return true;
		}
	}else if(typeid(*Download_Stop) == typeid(download_file)){
		DB_Download.terminate_download(Download_Stop->hash());
		delete Download_Stop;
		return false;
	}else{
		LOGGER << "unrecognized download type";
		exit(1);
	}
}
