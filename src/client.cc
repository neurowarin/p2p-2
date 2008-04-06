//boost
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

//std
#include <iostream>
#include <fstream>
#include <sstream>

#include "client.h"

client::client()
: FD_max(0), send_pending(new volatile int(0)), download_complete(new volatile int(0)),
stop_threads(false), threads(0), current_time(time(0)), previous_time(time(0))
{
	//create the download directory if it doesn't exist
	boost::filesystem::create_directory(global::CLIENT_DOWNLOAD_DIRECTORY);

	FD_ZERO(&master_FDS);
	Thread_Pool.init(global::NEW_CONN_THREADS);
	Download_Prep.init(download_complete);
}

client::~client()
{
	for(int socket_FD = 0; socket_FD <= FD_max; ++socket_FD){
		if(FD_ISSET(socket_FD, &master_FDS)){
			disconnect(socket_FD);
			Client_Buffer.erase(socket_FD);
		}
	}

	std::list<download *>::iterator iter_cur, iter_end;
	iter_cur = Download_Buffer.begin();
	iter_end = Download_Buffer.end();
	while(iter_cur != iter_end){
		delete *iter_cur;
		++iter_cur;
	}

	delete send_pending;
	delete download_complete;
}

void client::check_timeouts()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(CB_DB_mutex);
	for(int socket_FD = 0; socket_FD <= FD_max; ++socket_FD){
		if(FD_ISSET(socket_FD, &master_FDS)){
			if(current_time - Client_Buffer[socket_FD]->get_last_seen() >= global::TIMEOUT){
				global::debug_message(global::INFO,__FILE__,__FUNCTION__,"triggering timeout of socket ", socket_FD);
				disconnect(socket_FD);
				Client_Buffer.erase(socket_FD);
			}
		}
	}
	}//end lock scope
}

bool client::DC_add_existing(download_conn * DC)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(CB_DB_mutex);
	std::map<int, client_buffer *>::iterator iter_cur, iter_end;
	iter_cur = Client_Buffer.begin();
	iter_end = Client_Buffer.end();
	while(iter_cur != iter_end){
		if(DC->server_IP == iter_cur->second->get_IP()){
			//if download cancelled don't add it
			if(DC->Download == NULL){
				return true;
			}

			//client_buffer for the server found
			DC->Download->reg_conn(iter_cur->first, DC);  //register connection with download
			iter_cur->second->add_download(DC->Download); //register connection
			return true;
		}
		++iter_cur;
	}
	}//end lock scope
	return false;
}

void client::DC_block_concurrent(download_conn * DC)
{
	while(true){
		bool found = false;
		{//begin lock scope
		boost::mutex::scoped_lock lock(DC_mutex);
		std::list<download_conn *>::iterator iter_cur, iter_end;
		iter_cur = Connection_Current_Attempt.begin();
		iter_end = Connection_Current_Attempt.end();
		while(iter_cur != iter_end){
			if((*iter_cur)->server_IP == DC->server_IP){
				found = true;
				break;
			}
			++iter_cur;
		}

		if(!found){
			break;
		}
		}//end lock scope

		//exiting DC with same server_IP found, sleep then check again
		usleep(global::SPINLOCK_TIME);
	}
}

void client::DC_unblock(download_conn * DC)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(DC_mutex);
	std::list<download_conn *>::iterator iter_cur, iter_end;
	iter_cur = Connection_Current_Attempt.begin();
	iter_end = Connection_Current_Attempt.end();
	while(iter_cur != iter_end){
		if(*iter_cur == DC){
			Connection_Current_Attempt.erase(iter_cur);
			break;
		}
		++iter_cur;
	}
	}//end lock scope
}

inline void client::disconnect(const int & socket_FD)
{
	global::debug_message(global::INFO,__FILE__,__FUNCTION__, "disconnecting socket ", socket_FD);

	close(socket_FD);
	FD_CLR(socket_FD, &master_FDS);
	FD_CLR(socket_FD, &read_FDS);
	FD_CLR(socket_FD, &write_FDS);

	//reduce FD_max if possible
	for(int x = FD_max; x != 0; --x){
		if(FD_ISSET(x, &master_FDS)){
			FD_max = x;
			break;
		}
	}
}

bool client::get_download_info(std::vector<info_buffer> & download_info)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(CB_DB_mutex);
	if(Download_Buffer.size() == 0){
		return false;
	}

	std::list<download *>::iterator iter_cur, iter_end;
	iter_cur = Download_Buffer.begin();
	iter_end = Download_Buffer.end();
	while(iter_cur != iter_end){
		info_buffer info(
			(*iter_cur)->hash(),
			(*iter_cur)->name(),
			(*iter_cur)->total_size(),
			(*iter_cur)->speed(),
			(*iter_cur)->percent_complete()
		);
		(*iter_cur)->IP_list(info.server_IP);
		download_info.push_back(info);

		++iter_cur;
	}
	}//end lock scope

	return true;
}

int client::get_total_speed()
{
	Speed_Calculator.update(0);
	return Speed_Calculator.speed();
}

void client::main_thread()
{
	++threads;

	//reconnect downloads that havn't finished
	std::list<DB_access::download_info_buffer> resumed_download;
	DB_Access.download_initial_fill_buff(resumed_download);
	std::list<DB_access::download_info_buffer>::iterator iter_cur, iter_end;
	iter_cur = resumed_download.begin();
	iter_end = resumed_download.end();
	while(iter_cur != iter_end){
		start_download(*iter_cur);
		++iter_cur;
	}

	int recv_limit = 0;
	while(true){
		if(stop_threads){
			break;
		}

		current_time = time(0);
		if(current_time - previous_time > global::TIMEOUT){
			check_timeouts();
			previous_time = current_time;
		}

		if(*download_complete != 0){
			remove_complete();
		}

		prepare_requests();

		/*
		These must be initialized every iteration on linux(and possibly other OS's)
		because linux will change them(POSIX.1-2001 allows this) to reflect the
		time that select() has blocked for.
		*/
		timeval tv;
		tv.tv_sec = 1;
		tv.tv_usec = 0;

		/*
		This if(send_pending) exists to saves CPU time by not checking if sockets
		are ready to write when we don't need to write anything.
		*/
		if(*send_pending != 0){
			read_FDS = master_FDS;
			write_FDS = master_FDS;
			if((select(FD_max+1, &read_FDS, &write_FDS, NULL, &tv)) == -1){
				if(errno != EINTR){ //EINTR is caused by gprof
					perror("client select");
					exit(1);
				}
			}
		}
		else{
			FD_ZERO(&write_FDS);
			read_FDS = master_FDS;
			if((select(FD_max+1, &read_FDS, NULL, NULL, &tv)) == -1){
				if(errno != EINTR){ //EINTR is caused by gprof
					perror("client select");
					exit(1);
				}
			}
		}

		//process reads/writes
		char recv_buff[global::C_MAX_SIZE];
		int n_bytes;
		std::map<int, client_buffer *>::iterator CB_iter;
		for(int socket_FD = 0; socket_FD <= FD_max; ++socket_FD){
			if(FD_ISSET(socket_FD, &read_FDS)){
				if((recv_limit = Speed_Calculator.rate_control(global::DOWN_SPEED, global::C_MAX_SIZE)) != 0){
					if((n_bytes = recv(socket_FD, recv_buff, recv_limit, MSG_NOSIGNAL)) <= 0){
						if(n_bytes == -1){
							perror("client recv");
						}
						{//begin lock scope
						boost::mutex::scoped_lock lock(CB_DB_mutex);
						disconnect(socket_FD);
						Client_Buffer.erase(socket_FD);
						}//end lock scope
					}else{
						Speed_Calculator.update(n_bytes);
						{//begin lock scope
						boost::mutex::scoped_lock lock(CB_DB_mutex);
						CB_iter = Client_Buffer.find(socket_FD);
						CB_iter->second->recv_buff.append(recv_buff, n_bytes);
						CB_iter->second->post_recv();
						}//end lock scope
					}
				}
			}
			if(FD_ISSET(socket_FD, &write_FDS)){
				{//begin lock scope
				boost::mutex::scoped_lock lock(CB_DB_mutex);
				CB_iter = Client_Buffer.find(socket_FD);
				if(!CB_iter->second->send_buff.empty()){
					if((n_bytes = send(socket_FD, CB_iter->second->send_buff.c_str(), CB_iter->second->send_buff.size(), MSG_NOSIGNAL)) < 0){
						if(n_bytes == -1){
							perror("client send");
						}
					}else{ //remove bytes sent from buffer
						CB_iter->second->send_buff.erase(0, n_bytes);
						CB_iter->second->post_send();
					}
				}
				}//end lock scope
			}
		}
	}

	--threads;
}

void client::new_conn(download_conn * DC)
{
	hostent * he;

	{//begin lock scope
	boost::mutex::scoped_lock lock(gethostbyname_mutex);
	//resolve hostname
	he = gethostbyname(DC->server_IP.c_str());
	}//end lock scope

	if(he == NULL){
		herror("gethostbyname");
		return;
	}

	sockaddr_in dest_addr;
	DC->socket_FD = socket(PF_INET, SOCK_STREAM, 0);
	dest_addr.sin_family = AF_INET;
	dest_addr.sin_port = htons(global::P2P_PORT);
	dest_addr.sin_addr = *((struct in_addr *)he->h_addr_list[0]);
	memset(&(dest_addr.sin_zero),'\0',8);

	if(connect(DC->socket_FD, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) < 0){
		global::debug_message(global::INFO,__FILE__,__FUNCTION__,"connection to ",DC->server_IP, " failed");
		return;
	}else{
		global::debug_message(global::INFO,__FILE__,__FUNCTION__,"created socket ",DC->socket_FD," for ",DC->server_IP);
		if(DC->socket_FD > FD_max){
			FD_max = DC->socket_FD;
		}

		{//begin lock scope
		boost::mutex::scoped_lock lock(CB_DB_mutex);
		if(DC->Download == NULL){
			//connetion cancelled during connection, clean up and exit
			disconnect(DC->socket_FD);
			return;
		}else{
			Client_Buffer.insert(std::make_pair(DC->socket_FD, new client_buffer(DC->socket_FD, DC->server_IP, send_pending)));
			DC->Download->reg_conn(DC->socket_FD, DC);
			Client_Buffer[DC->socket_FD]->add_download(DC->Download);
		}
		}//end lock scope

		FD_SET(DC->socket_FD, &master_FDS);
		return;
	}
}

void client::prepare_requests()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(CB_DB_mutex);
	std::map<int, client_buffer *>::iterator iter_cur, iter_end;
	iter_cur = Client_Buffer.begin();
	iter_end = Client_Buffer.end();
	while(iter_cur != iter_end){
		iter_cur->second->prepare_request();
		++iter_cur;
	}
	}//end lock scope
}

void client::process_CQ()
{
	++threads;
	download_conn * DC;
	{//begin lock scope
	boost::mutex::scoped_lock lock(DC_mutex);
	if(Connection_Queue.empty()){
		return;
	}
	DC = Connection_Queue.front();
	Connection_Queue.pop_front();
	}//end lock scope

	//do not allow multiple DC with the same server_IP past
	DC_block_concurrent(DC);

	/*
	Add DC to an existing connection. Everything in here needs to account for the
	possibility of a download getting cancelled mid connection. If the download
	pointer in DC->Download is set to NULL it indicates a cancelled connection.
	*/
	if(!DC_add_existing(DC)){
		new_conn(DC);
	}

	//allow DC_block_concurrent to send next DC with same server_IP
	DC_unblock(DC);

	--threads;
}

void client::remove_complete()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(CB_DB_mutex);

	/*
	STEP 1:
	Determine what downloads are completed.
	*/
	std::list<download *> Complete_Download;
	std::list<download *>::iterator DB_iter_cur, DB_iter_end;
	DB_iter_cur = Download_Buffer.begin();
	DB_iter_end = Download_Buffer.end();
	while(DB_iter_cur != DB_iter_end){
		if((*DB_iter_cur)->complete()){
			Complete_Download.push_back(*DB_iter_cur);
		}
		++DB_iter_cur;
	}

	/*
	STEP 2:
	Remove any pending_connections for this download. In progress connections
	will be cancelled by setting their download pointer to NULL.
	*/
	std::list<download *>::iterator CD_iter_cur, CD_iter_end;
	CD_iter_cur = Complete_Download.begin();
	CD_iter_end = Complete_Download.end();
	while(CD_iter_cur != CD_iter_end){

		//pending connections
		std::deque<download_conn *>::iterator CQ_iter_cur, CQ_iter_end;
		CQ_iter_cur = Connection_Queue.begin();
		CQ_iter_end = Connection_Queue.end();
		while(CQ_iter_cur != CQ_iter_end){
			if((*CQ_iter_cur)->Download == *CD_iter_cur){
				CQ_iter_cur = Connection_Queue.erase(CQ_iter_cur);
			}else{
				++CQ_iter_cur;
			}
		}

		//in progress connections
		std::list<download_conn *>::iterator CQA_iter_cur, CQA_iter_end;
		CQA_iter_cur = Connection_Current_Attempt.begin();
		CQA_iter_end = Connection_Current_Attempt.end();
		while(CQA_iter_cur != CQA_iter_end){
			if((*CQA_iter_cur)->Download == *CD_iter_cur){
				(*CQA_iter_cur)->Download = NULL;
			}else{
				++CQA_iter_cur;
			}
		}

		++CD_iter_cur;
	}

	/*
	STEP 3:
	Terminate the completed downloads with all client_buffers.
	*/
	std::map<int, client_buffer *>::iterator CB_iter_cur, CB_iter_end;
	CB_iter_cur = Client_Buffer.begin();
	CB_iter_end = Client_Buffer.end();
	while(CB_iter_cur != CB_iter_end){
		CD_iter_cur = Complete_Download.begin();
		CD_iter_end = Complete_Download.end();
		while(CD_iter_cur != CD_iter_end){
			CB_iter_cur->second->terminate_download(*CD_iter_cur);
			++CD_iter_cur;
		}
		++CB_iter_cur;
	}

	/*
	STEP 4:
	Remove the download from the Download_Buffer and send it to Download_Prep.stop().
	*/
	CD_iter_cur = Complete_Download.begin();
	CD_iter_end = Complete_Download.end();
	while(CD_iter_cur != CD_iter_end){
		DB_iter_cur = Download_Buffer.begin();
		DB_iter_end = Download_Buffer.end();
		while(DB_iter_cur != DB_iter_end){
			if(*DB_iter_cur == *CD_iter_cur){


/*
DEBUG, it's possible this triggers another download start. Add in junk (it's the
junk, that makes the junkie) to account for that. Once Download is started and
associated servers registered the client_buffer will no longer get removed. This
will allow seamless transition from something like a download_hash_tree to a
download_file.

Use the above text for description.
*/

				Download_Prep.stop(*DB_iter_cur);
				Download_Buffer.erase(DB_iter_cur);
				break;
			}
			++DB_iter_cur;
		}
		++CD_iter_cur;
	}

	/*
	STEP 5:
	Disconnect empty client_buffer's.
	*/
	CB_iter_cur = Client_Buffer.begin();
	CB_iter_end = Client_Buffer.end();
	while(CB_iter_cur != CB_iter_end){
		if(CB_iter_cur->second->empty()){
			disconnect(CB_iter_cur->first);
			Client_Buffer.erase(CB_iter_cur);
			CB_iter_cur = Client_Buffer.begin();
		}else{
			++CB_iter_cur;
		}
	}

	*download_complete = 0;
	}//end lock scope
}

void client::start()
{
	boost::thread T1(boost::bind(&client::main_thread, this));
}

void client::stop()
{
	stop_threads = true;
	Thread_Pool.stop();
	while(threads){
		usleep(global::SPINLOCK_TIME);
	}
}

bool client::start_download(DB_access::download_info_buffer & info)
{
	download * Download;
	std::vector<download_conn *> servers;
	if(!(Download = Download_Prep.start_file(info, servers))){
		return false;
	}

	{//begin lock scope
	boost::mutex::scoped_lock lock(CB_DB_mutex);
	Download_Buffer.push_back(Download);
	}//end lock scope

	{//begin lock scope
	boost::mutex::scoped_lock lock(DC_mutex);
	std::vector<download_conn *>::iterator iter_cur, iter_end;
	iter_cur = servers.begin();
	iter_end = servers.end();
	while(iter_cur != iter_end){
		Connection_Queue.push_back(*iter_cur);

		//queue a job in the thread pool for the new connection
		void (client::*memfun_ptr)() = &client::process_CQ;
		Thread_Pool.queue_job(this, memfun_ptr);

		++iter_cur;
	}
	}//end lock scope

	return true;
}

void client::stop_download(const std::string & hash)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(CB_DB_mutex);
	std::list<download *>::iterator iter_cur, iter_end;
	iter_cur = Download_Buffer.begin();
	iter_end = Download_Buffer.end();
	while(iter_cur != iter_end){
		if(hash == (*iter_cur)->hash()){
			(*iter_cur)->stop();
			break;
		}
		++iter_cur;
	}
	}//end lock scope
}
