#ifndef H_DOWNLOAD_FACTORY
#define H_DOWNLOAD_FACTORY

//custom
#include "DB_download.h"

//download types
#include "client_buffer.h"
#include "download.h"
#include "download_connection.h"
#include "DB_search.h"
#include "DB_share.h"
#include "download_file.h"
#include "download_hash_tree.h"
#include "download_tracker.h"
#include "download_info.h"

//std
#include <fstream>
#include <list>
#include <typeinfo>

class download_factory
{
public:
	download_factory();

	/*
	start_file    - creates a download based on download_info_buffer
	                returns false if download already running, else true and Download set and servers filled
	stop          - stops all downloads, does different actions depending on download type
	                returns false if no download needs to be started in response to the download stopping
	                if true then Download_start will be set and servers filled
	*/
	bool start_hash(const download_info & info, download *& Download, std::list<download_connection> & servers);
	bool stop(download * Download_Stop, download *& Download_Start, std::list<download_connection> & servers);

private:
	/*
	start_file - starts a download_file
	*/
	download_file * start_file(download_hash_tree * DHT, std::list<download_connection> & servers);

	DB_download DB_Download;
	DB_search DB_Search;
	DB_share DB_Share;
};
#endif
