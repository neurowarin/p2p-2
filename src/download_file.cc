#include "download_file.h"

download_file::download_file(
	const std::string & file_hash_in,
	const std::string & file_name_in, 
	const std::string & file_path_in,
	const uint64_t & file_size_in
):
	file_hash(file_hash_in),
	file_name(file_name_in),
	file_path(file_path_in),
	file_size(file_size_in),
	hashing(true),
	hash_latest(0),
	threads(0),
	stop_threads(false),
	thread_file_hash(file_hash_in),
	thread_file_path(file_path_in),
	download_complete(false),
	close_slots(false)
{
	/*
	Block requests start at zero so the lst block is the number of file block
	hashes there is minus one.
	*/
	if(file_size % global::FILE_BLOCK_SIZE == 0){
		//exact block count, subtract one to get last_block number
		last_block = file_size / global::FILE_BLOCK_SIZE - 1;
	}else{
		//partial last block (decimal gets truncated which is effectively minus one)
		last_block = file_size / global::FILE_BLOCK_SIZE;
	}

	/*
	If the file size is exactly divisible by global::FILE_BLOCK_SIZE there is no
	partial last block.
	*/
	if(file_size % global::FILE_BLOCK_SIZE == 0){
		//last block exactly global::FILE_BLOCKS_SIZE + command size
		last_block_size = global::FILE_BLOCK_SIZE + 1;
	}else{
		//partial last block + command size
		last_block_size = file_size % global::FILE_BLOCK_SIZE + 1;
	}

	std::fstream fin((global::DOWNLOAD_DIRECTORY+file_name).c_str(), std::ios::in);
	assert(fin.is_open());
	fin.seekg(0, std::ios::end);
	hash_last = fin.tellg() / global::FILE_BLOCK_SIZE;

	//hash check file blocks
	boost::thread T(boost::bind(&download_file::hash_check, this));
}

download_file::~download_file()
{
	stop_threads = true;
	while(threads){
		usleep(1);
	}
}

bool download_file::complete()
{
	return download_complete;
}

const std::string & download_file::hash()
{
	return file_hash;
}

void download_file::hash_check()
{
	++threads;
	std::fstream fin;
	fin.open(thread_file_path.c_str(), std::ios::in);
	assert(fin.is_open());

	char block_buff[global::FILE_BLOCK_SIZE];
	while(true){
		fin.read(block_buff, global::FILE_BLOCK_SIZE);
		if(fin.gcount() == 0){
			break;
		}
		if(!Hash_Tree.check_block(thread_file_hash, (uint64_t)hash_latest, block_buff, fin.gcount())){
			break;
		}
		++hash_latest;
		if(stop_threads){
			break;
		}
	}

	Request_Gen.init((uint64_t)hash_latest, last_block, global::RE_REQUEST_TIMEOUT);
	hashing = false;
	--threads;
}

const std::string download_file::name()
{
	if(hashing){
		return file_name + " CHECKING";
	}else{
		return file_name;
	}
}

unsigned int download_file::percent_complete()
{
	if(last_block == 0){
		return 0;
	}else{
		if(hashing){
			return (unsigned int)(((float)hash_latest / (float)hash_last)*100);
		}else{
			return (unsigned int)(((float)Request_Gen.highest_requested() / (float)last_block)*100);
		}
	}
}

bool download_file::request(const int & socket, std::string & request, std::vector<std::pair<char, int> > & expected)
{
/*
Think about letting requests happen while hashing is going on. To do this have
the hashing function force_rerequest blocks that it find that are bad. Initialize
the Request_Gen immediately.

Request_Gen may have to be made thread safe. This can be internal thread safety
with mutex on all public member functions.
*/


	if(download_complete || hashing){
		return false;
	}

	std::map<int, download_conn *>::iterator iter = Connection.find(socket);
	assert(iter != Connection.end());
	download_file_conn * conn = (download_file_conn *)iter->second;

	if(!conn->slot_ID_requested){
		//slot_ID not yet obtained from server
		request = global::P_REQUEST_SLOT_FILE + hex::hex_to_binary(file_hash);
		conn->slot_ID_requested = true;
		expected.push_back(std::make_pair(global::P_SLOT_ID, global::P_SLOT_ID_SIZE));
		expected.push_back(std::make_pair(global::P_ERROR, 1));
		return true;
	}else if(!conn->slot_ID_received){
		//slot_ID requested but not yet received
		return false;
	}else if(close_slots && !conn->close_slot_sent){
		/*
		The file finished downloading or the download was manually stopped. This
		server has not been sent P_CLOSE_SLOT.
		*/
		request += global::P_CLOSE_SLOT;
		request += conn->slot_ID;
		conn->close_slot_sent = true;
		bool unready_found = false;
		std::map<int, download_conn *>::iterator iter_cur, iter_end;
		iter_cur = Connection.begin();
		iter_end = Connection.end();
		while(iter_cur != iter_end){
			if(((download_file_conn *)iter_cur->second)->close_slot_sent == false){
				unready_found = true;
			}
			++iter_cur;
		}
		if(!unready_found){
			download_complete = true;
		}
		return true;
	}else if(!Request_Gen.request(conn->latest_request)){
		//no request to be made at the moment
		return false;
	}

	request += global::P_SEND_BLOCK;
	request += conn->slot_ID;
	request += Convert_uint64.encode(conn->latest_request.back());
	int size;
	if(conn->latest_request.back() == last_block){
		size = last_block_size;
	}else{
		size = global::P_BLOCK_SIZE;
	}

	expected.push_back(std::make_pair(global::P_BLOCK, size));
	expected.push_back(std::make_pair(global::P_ERROR, 1));
	return true;
}

void download_file::response(const int & socket, std::string block)
{
	download_file_conn * conn = (download_file_conn *)Connection[socket];

	if(block[0] == global::P_SLOT_ID && conn->slot_ID_received == false){
		conn->slot_ID = block[1];
		conn->slot_ID_received = true;
	}else if(block[0] == global::P_BLOCK){
		//a block was received
		conn->Speed_Calculator.update(block.size()); //update server speed
		Speed_Calculator.update(block.size());       //update download speed
		block.erase(0, 1); //trim command
		if(!Hash_Tree.check_block(file_hash, conn->latest_request.front(), block.c_str(), block.length())){
			Request_Gen.force_re_request(conn->latest_request.front());
			logger::debug(LOGGER_P1,file_name,":",conn->latest_request.front()," hash failure");
		}else{
			if(!close_slots){
				write_block(conn->latest_request.front(), block);
			}
			Request_Gen.fulfil(conn->latest_request.front());
		}
		conn->latest_request.pop_front();
		if(Request_Gen.complete()){
			//download is complete, start closing slots
			close_slots = true;
		}
	}
}

void download_file::stop()
{
	namespace fs = boost::filesystem;
	fs::path path = fs::system_complete(fs::path(file_path, fs::native));
	fs::remove(path);

	if(Connection.size() == 0){
		download_complete = true;
	}else{
		close_slots = true;
	}
}

void download_file::write_block(uint64_t block_number, std::string & block)
{
	std::fstream fout(file_path.c_str(), std::ios::in | std::ios::out | std::ios::binary);
	if(fout.is_open()){
		fout.seekp(block_number * global::FILE_BLOCK_SIZE, std::ios::beg);
		fout.write(block.c_str(), block.size());
	}else{
		logger::debug(LOGGER_P1,"error opening file ",file_path);
	}
}

const uint64_t download_file::size()
{
	return file_size;
}
