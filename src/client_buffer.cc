#include <iostream>

#include "client_buffer.h"
#include "conversion.h"

client_buffer::client_buffer(int socket_in, std::string & server_IP_in, atomic<int> * send_pending_in)
{
	socket = socket_in;
	server_IP = server_IP_in;
	send_pending = send_pending_in;
	recv_buff.reserve(global::BUFFER_SIZE);
	send_buff.reserve(global::C_CTRL_SIZE);
	Download_iter = Download.begin();
	ready = true;
	abuse = false;
	terminating = false;
	last_seen = time(0);
}

client_buffer::~client_buffer()
{
	unreg_all();
}

void client_buffer::add_download(download * new_download)
{
	Download.push_back(new_download);
}

const bool client_buffer::empty()
{
	return Download.empty();
}

const std::string & client_buffer::get_IP()
{
	return server_IP;
}

const time_t & client_buffer::get_last_seen()
{
	return last_seen;
}

void client_buffer::post_recv()
{
	if(recv_buff.size() == bytes_expected){
		(*Download_iter)->response(socket, recv_buff);
		recv_buff.clear();
		ready = true;
	}

	if(recv_buff.size() > bytes_expected){
		abuse = true;
	}

	last_seen = time(0);
}

void client_buffer::post_send()
{
	if(send_buff.empty()){
		--(*send_pending);
	}
}

void client_buffer::prepare_request()
{
	if(ready && !terminating){
		rotate_downloads();
		(*Download_iter)->request(socket, send_buff);
		bytes_expected = (*Download_iter)->bytes_expected();
		++(*send_pending);
		ready = false;
	}
}

void client_buffer::rotate_downloads()
{
	if(!Download.empty()){
		++Download_iter;
	}

	if(Download_iter == Download.end()){
		Download_iter = Download.begin();
	}
}

const bool client_buffer::terminate_download(const std::string & hash)
{
	if(hash == (*Download_iter)->hash()){
		if(ready){ //if not expecting any more bytes
			(*Download_iter)->unreg_conn(socket);
			Download_iter = Download.erase(Download_iter);
			terminating = false;
			return true;
		}
		else{ //expecting more bytes, let them finish before terminating
			terminating = true;
			return false;
		}
	}

	std::list<download *>::iterator iter_cur, iter_end;
	iter_cur = Download.begin();
	iter_end = Download.end();
	while(iter_cur != iter_end){
		if(hash == (*iter_cur)->hash()){
			(*iter_cur)->unreg_conn(socket);
			Download.erase(iter_cur);
			break;
		}
		++iter_cur;
	}

	return true;
}

void client_buffer::unreg_all()
{
	std::list<download *>::iterator iter_cur, iter_end;
	iter_cur = Download.begin();
	iter_end = Download.end();
	while(iter_cur != iter_end){
		(*iter_cur)->unreg_conn(socket);
		++iter_cur;
	}
}

