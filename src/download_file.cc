//boost
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

//std
#include <fstream>
#include <iostream>
#include <sstream>
#include <time.h>

#include "download_file.h"

download_file::download_file(const std::string & file_hash_in, const std::string & file_name_in, 
	const std::string & file_path_in, const int & file_size_in, const int & latest_request_in,
	const int & last_block_in, const int & last_block_size_in, const int & last_super_block_in,
	const int & current_super_block_in, atomic<bool> * download_complete_flag_in)
{
	//non-defaults
	file_hash = file_hash_in;
	file_name = file_name_in;
	file_path = file_path_in;
	file_size = file_size_in;
	latest_request = latest_request_in;
	last_block = last_block_in;
	last_block_size = last_block_size_in;
	last_super_block = last_super_block_in;
	current_super_block = current_super_block_in;
	download_complete_flag = download_complete_flag_in;

	//defaults
	download_complete = false;

#ifdef UNRELIABLE_CLIENT
	std::srand(time(0));
#endif

	//set the hash type
	SHA.Init(global::HASH_TYPE);
}

bool download_file::complete()
{
	return download_complete;
}

const int download_file::bytes_expected()
{
	if(latest_request == last_block){
		return last_block_size;
	}
	else{
		return global::BUFFER_SIZE;
	}
}

const std::string & download_file::hash()
{
	return file_hash;
}

void download_file::IP_list(std::vector<std::string> & list)
{
	std::map<int, connection>::iterator iter_cur, iter_end;
	iter_cur = Connection.begin();
	iter_end = Connection.end();
	while(iter_cur != iter_end){
		list.push_back(iter_cur->second.server_IP);
		++iter_cur;
	}
}

const std::string & download_file::name()
{
	return file_name;
}

int download_file::percent_complete()
{
	return (int)(((latest_request * global::BUFFER_SIZE)/(float)file_size)*100);
}

bool download_file::request(const int & socket, std::string & request)
{
	std::map<int, connection>::iterator iter = Connection.find(socket);

	if(iter == Connection.end()){
#ifdef DEBUG
	std::cout << "info: download_file::request() socket not registered";
#endif
		return false;
	}

	iter->second.latest_request = needed_block();
	request = global::P_SBL + conversion::encode_int(iter->second.file_ID) + conversion::encode_int(iter->second.latest_request);
	return true;
}

bool download_file::response(const int & socket, std::string & block)
{
	std::map<int, connection>::iterator iter = Connection.find(socket);

	if(iter == Connection.end()){
		std::cout << "info: download_file::response() socket not registered\n";
		return false;
	}

#ifdef UNRELIABLE_CLIENT
	int random = rand() % 100;
	if(random < global::UNRELIABLE_CLIENT_VALUE){
		std::cout << "testing: client::add_block(): OOPs! I dropped fileBlock " << iter->second.latest_request << "\n";
		if(iter->second.latest_request == last_block){
			block.clear();
		}
		else{
			block.erase(0, global::BUFFER_SIZE);
		}
	}
	else{
#endif

	//trim off protocol information
	block.erase(0, global::S_CTRL_SIZE);

	//add the block to the appropriate super_block
	for(std::deque<super_block>::iterator iter0 = Super_Buffer.begin(); iter0 != Super_Buffer.end(); ++iter0){
		if(iter0->add_block(iter->second.latest_request, block)){
			break;
		}
	}
#ifdef UNRELIABLE_CLIENT
	}
#endif

	//the Super_Buffer may be empty if the last block is requested more than once
	if(iter->second.latest_request == last_block && !Super_Buffer.empty()){
		write_super_block(Super_Buffer.back().container);
		Super_Buffer.pop_back();
		download_complete = true;
		*download_complete_flag = true;
	}

	calculate_speed();

	return true;
}

unsigned int download_file::needed_block()
{
	/*
	Write out the oldest superBlocks if they're complete. It's possible the
	Super_Buffer could be empty so a check for this is done.
	*/
	if(!Super_Buffer.empty()){
		while(Super_Buffer.back().complete()){

			write_super_block(Super_Buffer.back().container);
			Super_Buffer.pop_back();

			if(Super_Buffer.empty()){
				break;
			}
		}
	}

	/*
	SB = super_block
	FB = fileBlock
	* = requested fileBlock
	M = missing fileBlock

	Scenario #1 Ideal Conditions:
		P1 would load a SB and return the first request. P4 would serve all requests
	until SB was half completed. P3 would serve all requests until all requests
	made. At this point either P1 or P2 will get called depending on whether
	all FB were received for SB. This cycle would repeat under ideal conditions.

	Scenario #2 Unreliable Hosts:
		P1 would load a SB and return the first request. P4 would serve all requests
	until SB was half completed. P3 would serve all requests until all requests
	made. At this point we're missing FB that slow servers havn't sent yet.
	P2 adds a leading SB to the buffer. P4 requests FB for the leading SB until
	half the FB are requested. At this point P3 would rerequest FB from the back
	until those blocks were gotten. The back SB would be written as soon as it
	completed and P3 would start serving requests from the one remaining SB.

	Example of a rerequest:
	 back()                     front()
	-----------------------	   -----------------------
	|***M*******M*********| 	|***M******           |
	-----------------------   	-----------------------
	At this point the missing blocks from the back() would be rerequested.

	Example of new SB creation.
	 front() = back()
	-----------------------
	|*******************M*|
	-----------------------
	At this point a new leading SB would be added and that missing FB would be
	given time to arrive.
	*/
	if(Super_Buffer.empty()){ //P1
		Super_Buffer.push_front(super_block(current_super_block++, last_block));
		latest_request = Super_Buffer.front().get_request();
	}
	else if(Super_Buffer.front().all_requested()){ //P2
		if(Super_Buffer.size() < 2 && current_super_block <= last_super_block){
			Super_Buffer.push_front(super_block(current_super_block++, last_block));
		}
		latest_request = Super_Buffer.front().get_request();
	}
	else if(Super_Buffer.front().half_requested()){ //P3
		latest_request = Super_Buffer.back().get_request();
	}
	else{ //P4
		latest_request = Super_Buffer.front().get_request();
	}

	/*
	Don't return the last block number until it's the absolute last needed by
	this download. This makes checking for a completed download a lot easier!
	*/
	if(latest_request == last_block){

		//determine if there are no superBlocks left but the last one
		if(Super_Buffer.size() == 1){

			/*
			If no super_block but the last one remains then try getting another
			request. If the super_block returns -1 then it doesn't need any other
			blocks.
			*/
			int request = Super_Buffer.back().get_request();

			//if no other block but the last one is needed
			if(request == -1){
				return last_block;
			}
			else{ //if another block is needed
				latest_request = request;
				return latest_request;
			}
		}
		else{ //more than one super_block, serve request from oldest
			latest_request = Super_Buffer.back().get_request();
			return latest_request;
		}
	}
	else{ //any request but the last one gets returned without the above checking
		return latest_request;
	}
}

void download_file::stop()
{
	namespace fs = boost::filesystem;

	download_complete = true;
	Super_Buffer.clear();
	fs::path path = fs::system_complete(fs::path(file_path, fs::native));
	fs::remove(path);
	*download_complete_flag = true;
}

void download_file::write_super_block(std::string container[])
{
/*
	//get the messageDigest of the super_block
	for(int x=0; x<global::SUPERBLOCK_SIZE; ++x){
		SHA.Update((const sha_byte *) container[x].c_str(), container[x].size());
	}
	SHA.End();
	std::cout << SHA.StringHash() << "\n";
*/
	std::ofstream fout(file_path.c_str(), std::ios::app);

	if(fout.is_open()){
		for(int x=0; x<global::SUPERBLOCK_SIZE; ++x){
			fout.write(container[x].c_str(), container[x].size());
		}
		fout.close();
	}
	else{
#ifdef DEBUG
		std::cout << "error: download_file::writeTree() error opening file\n";
#endif
	}
}

void download_file::reg_conn(int socket, std::string & server_IP, unsigned int file_ID)
{
	connection temp(server_IP, file_ID);
	Connection.insert(std::make_pair(socket, temp));
}

const int & download_file::total_size()
{
	return file_size;
}

void download_file::unreg_conn(const int & socket)
{
	Connection.erase(socket);
}
