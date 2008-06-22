#ifndef H_CLIENT_BUFFER
#define H_CLIENT_BUFFER

//boost
#include <boost/thread/mutex.hpp>

//custom
#include "download_file.h"
#include "download_hash_tree.h"
#include "convert.h"

//std
#include <ctime>
#include <deque>
#include <iostream>
#include <list>
#include <map>
#include <set>
#include <typeinfo>

//custom
#include "download.h"
#include "global.h"

class client_buffer
{
public:
	client_buffer(const int & socket_in, const std::string & IP_in);
	~client_buffer();

	/*
	Add a download to the client_buffer. Doesn't associate the download with any
	instantiation of client_buffer.
	*/
	static void add_download(download * Download)
	{
		boost::mutex::scoped_lock lock(Mutex);
		Unique_Download.insert(Download);
	}

	/*
	Removes client_buffers that have timed out. Puts sockets to disconnect in
	timed_out vector.
	*/
	static void check_timeouts(std::vector<int> & timed_out)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::map<int, client_buffer *>::iterator iter_cur, iter_end;
		iter_cur = Client_Buffer.begin();
		iter_end = Client_Buffer.end();
		while(iter_cur != iter_end){
			if(time(0) - iter_cur->second->last_seen >= global::TIMEOUT){
				timed_out.push_back(iter_cur->first);
				delete iter_cur->second;
				Client_Buffer.erase(iter_cur);
				iter_cur = Client_Buffer.begin();
			}else{
				++iter_cur;
			}
		}
	}

	/*
	Populates the info vector with download information for all downloads running
	in all client_buffers. The vector passed in is cleared before download info is
	added.
	*/
	static void current_downloads(std::vector<download_info> & info)
	{
		boost::mutex::scoped_lock lock(Mutex);
		info.clear();
		std::set<download *>::iterator iter_cur, iter_end;
		iter_cur = Unique_Download.begin();
		iter_end = Unique_Download.end();
		while(iter_cur != iter_end){
			if(typeid(**iter_cur) == typeid(download_hash_tree) || typeid(**iter_cur) == typeid(download_file)){
				download_info Download_Info(
					(*iter_cur)->hash(),
					(*iter_cur)->name(),
					(*iter_cur)->size(),
					(*iter_cur)->speed(),
					(*iter_cur)->percent_complete()
				);
				(*iter_cur)->IP_list(Download_Info.IP);
				info.push_back(Download_Info);
			}
			++iter_cur;
		}
	}

	/*
	Will try to add download to an existing client_buffer. Returns true if client_buffer
	for download_conn found, else returns false. If false is returned then new_connection()
	should be called to create a new client_buffer for the download_conn.
	*/
	static bool DC_add_existing(download_conn * DC)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::map<int, client_buffer *>::iterator iter_cur, iter_end;
		iter_cur = Client_Buffer.begin();
		iter_end = Client_Buffer.end();
		while(iter_cur != iter_end){
			if(DC->IP == iter_cur->second->IP){
				if(DC->Download == NULL){
					//download cancelled, abort adding it
					return true;
				}
				DC->socket = iter_cur->second->socket;             //set socket of the discovered client_buffer
				DC->Download->register_connection(DC);             //register connection with download
				iter_cur->second->register_download(DC->Download); //register download with client_buffer
				return true;
			}
			++iter_cur;
		}
		return false;
	}

	/*
	Deletes all downloads in client_buffer and deletes all client_buffer instantiations.
	This is used when client is being destroyed.
	*/
	static void destroy()
	{
		while(!Unique_Download.empty()){
			delete *Unique_Download.begin();
			Unique_Download.erase(Unique_Download.begin());
		}
		while(!Client_Buffer.empty()){
			delete Client_Buffer.begin()->second;
			Client_Buffer.erase(Client_Buffer.begin());
		}
	}

	/*
	Erases an element from Client_Buffer associated with socket_FD. This should be
	called whenever a socket is disconnected.
	*/
	static void erase(const int & socket_FD)
	{
		boost::mutex::scoped_lock lock(Mutex);
		Client_Buffer.erase(socket_FD);
	}

	/*
	Populates the complete list with pointers to downloads which are completed.
	WARNING: Do not do anything with these pointers but compare them by value. Any
	         dereferencing of these pointers is NOT thread safe.
	*/
	static void find_complete(std::list<download *> & complete)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::set<download *>::iterator iter_cur, iter_end;
		iter_cur = Unique_Download.begin();
		iter_end = Unique_Download.end();
		while(iter_cur != iter_end){
			if((*iter_cur)->complete()){
				complete.push_back(*iter_cur);
			}
			++iter_cur;
		}
	}

	/*
	Causes all client_buffers to query their downloads to see if they need to make
	new requests.
	*/
	static void generate_requests()
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::map<int, client_buffer *>::iterator iter_cur, iter_end;
		iter_cur = Client_Buffer.begin();
		iter_end = Client_Buffer.end();
		while(iter_cur != iter_end){
			iter_cur->second->prepare_request();
			++iter_cur;
		}
	}

	/*
	Returns the counter that indicates whether a send needs to be done. If it is
	zero no send needs to be done. Every +1 above zero it is indicates a send_buff
	has something in it.
	*/
	static const volatile int & get_send_pending()
	{
		return send_pending;
	}

	/*
	Returns the client_buffer send_buff that corresponds to socket_FD.
	WARNING: violates encapsulation, NOT thread safe to call this from multiple threads
	*/
	static std::string & get_send_buff(const int & socket_FD)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::map<int, client_buffer *>::iterator iter = Client_Buffer.find(socket_FD);
		assert(iter != Client_Buffer.end());
		return iter->second->send_buff;
	}

	/*
	Returns the client_buffer send_buff that corresponds to socket_FD.
	WARNING: violates encapsulation, NOT thread safe to call this from multiple threads
	*/
	static std::string & get_recv_buff(const int & socket_FD)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::map<int, client_buffer *>::iterator iter = Client_Buffer.find(socket_FD);
		assert(iter != Client_Buffer.end());
		return iter->second->recv_buff;
	}

	/*
	Creates new client_buffer. Registers the download with the client_buffer.
	Registers the download_conn with the download.
	*/
	static void new_connection(download_conn * DC)
	{
		boost::mutex::scoped_lock lock(Mutex);
		Client_Buffer.insert(std::make_pair(DC->socket, new client_buffer(DC->socket, DC->IP)));
		Client_Buffer[DC->socket]->register_download(DC->Download);
		DC->Download->register_connection(DC);
	}

	/*
	Should be called after recieving data. This function determines if there are
	any complete commands and if there are it slices them off the recv_buff and
	sends them to the downloads.
	*/
	static void post_recv(const int & socket_FD)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::map<int, client_buffer *>::iterator iter = Client_Buffer.find(socket_FD);
		assert(iter != Client_Buffer.end());
		iter->second->post_recv();
	}

	/*
	Should be called after sending data. This function evaluates whether or not the
	send_buff was emptied. If it was then it decrements send_pending.
	*/
	static void post_send(const int & socket_FD)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::map<int, client_buffer *>::iterator iter = Client_Buffer.find(socket_FD);
		assert(iter != Client_Buffer.end());
		iter->second->post_send();
	}

	/*
	Removes all empty client_buffers and returns their socket numbers which need
	to be disconnected.
	*/
	static void remove_empty(std::vector<int> & disconnect_sockets)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::map<int, client_buffer *>::iterator iter_cur, iter_end;
		iter_cur = Client_Buffer.begin();
		iter_end = Client_Buffer.end();
		while(iter_cur != iter_end){
			if(iter_cur->second->empty()){
				disconnect_sockets.push_back(iter_cur->first);
				delete iter_cur->second;
				Client_Buffer.erase(iter_cur);
				iter_cur = Client_Buffer.begin();
			}else{
				++iter_cur;
			}
		}
	}

	/*
	Remove the download from the client_buffer.
	*/
	static void remove_download(download * Download)
	{
		boost::mutex::scoped_lock lock(Mutex);
		Unique_Download.erase(Download);
		std::map<int, client_buffer *>::iterator iter_cur, iter_end;
		iter_cur = Client_Buffer.begin();
		iter_end = Client_Buffer.end();
		while(iter_cur != iter_end){
			iter_cur->second->terminate_download(Download);
			++iter_cur;
		}
	}

	//stops the download associated with hash
	static void stop_download(const std::string & hash)
	{
		boost::mutex::scoped_lock lock(Mutex);
		std::set<download *>::iterator iter_cur, iter_end;
		iter_cur = Unique_Download.begin();
		iter_end = Unique_Download.end();
		while(iter_cur != iter_end){
			if(hash == (*iter_cur)->hash()){
				(*iter_cur)->stop();
				break;
			}
			++iter_cur;
		}
	}

private:
	static boost::mutex Mutex;                           //mutex for any access to a download
	static std::map<int, client_buffer *> Client_Buffer; //socket mapped to client_buffer
	static std::set<download *> Unique_Download;         //all current downloads

	/*
	When this is zero there are no pending requests to send. This exists for the
	purpose of having an alternate select() call that doesn't involve write_FDS
	because using write_FDS hogs CPU. When the value referenced by this equals 0
	then there are no sends pending and write_FDS doesn't need to be used. These
	are given to the client_buffer elements so they can increment it.
	*/
	static volatile int send_pending;

	std::string recv_buff;
	std::string send_buff;

	/*
	The Download container is effectively a ring. The rotate_downloads() function will move
	Download_iter through it in a circular fashion.
	*/
	std::list<download *> Download;                //all downloads that this client_buffer is serving
	std::list<download *>::iterator Download_iter; //last download a request was gotten from

	std::string IP;   //IP associated with this client_buffer
	int socket;       //socket number of this element
	time_t last_seen; //used for timeout
	bool abuse;       //if true a disconnect is triggered

	class pending_response
	{
	public:
		//possible responses paired with size of the possible response
		std::vector<std::pair<char, int> > expected;
		download * Download;
	};

	/*
	Past requests are stored here. The front of this queue will contain the oldest
	requests. When a response comes in it will correspond to the pending_response
	in the front of this queue.
	*/
	std::deque<pending_response> Pipeline;

	/*
	empty              - return true if client_buffer contains no downloads
	post_recv          - does actions which may need to be done after a recv
	post_send          - should be called after every send
	prepare_request    - if another request it needed rotate downloads and get a request
	register_download  - associate a download with this client_buffer
	rotate_downloads   - moves Download_iter through Download in a circular fashion
	                     returns true if looped back to beginning
	terminate_download - removes the download which corresponds to hash
	unregister_all     - unregisters the connection with all available downloads
	                     called from destructor to unregister from all downloads
	                       when the client_buffer is destroyed
	*/
	bool empty();
	void post_recv();
	void post_send();
	void prepare_request();
	void register_download(download * new_download);
	bool rotate_downloads();
	void terminate_download(download * term_DL);
	void unregister_all();
};
#endif
