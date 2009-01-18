#ifndef H_SERVER
#define H_SERVER

//boost
#include <boost/thread/thread.hpp>
#include <boost/bind.hpp>

//C
#include <errno.h>
#include <signal.h>

//custom
#include "atomic_int.h"
#include "DB_blacklist.h"
#include "DB_preferences.h"
#include "global.h"
#include "rate_limit.h"
#include "server_buffer.h"
#include "server_index.h"
#include "upload_info.h"

//networking
#ifdef WIN32
#define FD_SETSIZE 1024 //max number of connections in FD_SET
#include <winsock.h>
#define socklen_t int
#define MSG_NOSIGNAL 0
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

//std
#include <ctime>
#include <deque>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <sstream>
#include <string>
#include <vector>

class server
{
public:
	server();
	~server();

	/*
	current_uploads     - returns current upload information
	get_max_connections - returns maximum connections server will accept
	get_share_directory - returns the location to be shared/indexed
	get_upload_rate     - returns the download speed limit (B/s)
	is_indexing         - returns true if the server is indexing files
	set_max_connections - sets maximum connections server will accept
	set_share_directory - sets the share directory
	set_max_upload_rate - sets a new upload rate limit (B/s)
	total_rate          - returns total upload rate (B/s)
	*/
	void current_uploads(std::vector<upload_info> & info);
	unsigned get_max_connections();
	std::string get_share_directory();
	unsigned get_upload_rate();
	bool is_indexing();
	void set_max_connections(const unsigned & max_connections_in);
	void set_share_directory(const std::string & share_directory);
	void set_max_upload_rate(unsigned upload_rate);
	unsigned total_rate();

private:
	boost::thread main_thread;

	int connections;     //currently established connections
	int max_connections; //connection limit

	//select() related
	fd_set master_FDS;   //master file descriptor set
	fd_set read_FDS;     //set when socket can read without blocking
	fd_set write_FDS;    //set when socket can write without blocking
	int FD_max;          //holds the number of the maximum socket

	//passed to DB_Blacklist to check for updates to blacklist
	int blacklist_state;

	/*
	check_blacklist - disconects servers which have been blacklisted
	disconnect      - disconnect client and remove socket from master set
	main_thread     - where the party is at
	new_conn        - sets up socket for client
	process_request - adds received bytes to buffer and interprets buffer
	*/
	void check_blacklist();
	void disconnect(const int & socketfd);
	void main_loop();
	void new_connection(const int & listener);
	void process_request(const int & socket_FD, char * recv_buff, const int & n_bytes);

	DB_blacklist DB_Blacklist;
	DB_preferences DB_Preferences;
	rate_limit Rate_Limit;
	server_index Server_Index;
};
#endif
