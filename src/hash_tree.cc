//boost
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/path.hpp>

//std
#include <fstream>
#include <iostream>

#include "hash_tree.h"

hash_tree::hash_tree()
: stop_thread(false), SHA(global::P_BLOCK_SIZE - 1)
{
	//create hash directory if it doesn't already exist
	boost::filesystem::create_directory(global::HASH_DIRECTORY.c_str());

	block_size = global::P_BLOCK_SIZE - 1;
}

bool hash_tree::check_exists(std::string root_hash)
{
	std::fstream hash_fstream((global::HASH_DIRECTORY+root_hash).c_str(), std::ios::in);
	return hash_fstream.is_open();
}

inline bool hash_tree::check_hash(char parent[], char left_child[], char right_child[])
{
	SHA.init();
	SHA.load(left_child, sha::HASH_LENGTH);
	SHA.load(right_child, sha::HASH_LENGTH);
	SHA.end();

	if(memcmp(parent, SHA.raw_hash(), sha::HASH_LENGTH) == 0){
		return true;
	}else{
		return false;
	}
}

bool hash_tree::check_hash_tree(std::string root_hash, unsigned long hash_count, std::pair<unsigned long, unsigned long> & bad_hash)
{
	//reset the positions if checking a different tree
	if(check_tree_latest.empty() || check_tree_latest != root_hash){
		current_RRN = 0; //RRN of latest hash retrieved
		start_RRN = 0;   //RRN of the start of the current row
		end_RRN = 0;     //RRN of the end of the current row
	}

	std::fstream hash_fstream((global::HASH_DIRECTORY+root_hash).c_str(), std::fstream::in);

	//hash tree does not yet exist
	if(!hash_fstream.is_open()){
		bad_hash.first = 0;
		bad_hash.second = 1;
		return true;
	}

	//a null hash represends the early termination of a row
	char end_of_row[sha::HASH_LENGTH];
	memset(end_of_row, '\0', sha::HASH_LENGTH);

	//buffers for a parent and two child nodes
	char hash[sha::HASH_LENGTH];
	char left_child[sha::HASH_LENGTH];
	char right_child[sha::HASH_LENGTH];

	bool early_termination = false;
	while(true){
		hash_fstream.seekg(current_RRN * sha::HASH_LENGTH);
		hash_fstream.read(hash, sha::HASH_LENGTH);

		//detect incomplete/missing parent hash
		if(hash_fstream.eof()){
			bad_hash.first = current_RRN;
			bad_hash.second = current_RRN + 1;
			return true;
		}

		//detect early termination of a row
		if(memcmp(hash, end_of_row, sha::HASH_LENGTH) == 0){
			early_termination = true;
			++current_RRN;
			continue;
		}

		//calculate start and end RRNs of the current row
		if(current_RRN == end_RRN + 1){
			unsigned long start_RRN_temp = start_RRN;
			start_RRN = end_RRN + 1;
			end_RRN = 2 * (end_RRN - start_RRN_temp) + current_RRN + 1;

			if(early_termination){
				end_RRN -= 2;
				early_termination = false;
			}
		}

		unsigned long first_child_RRN = 2 * (current_RRN - start_RRN) + end_RRN + 1;

		//stopping case, all hashes in the second row checked, bottom file hash row good
		if(first_child_RRN + 1 >= hash_count){
			hash_fstream.close();
			return false;
		}

		//read child nodes
		hash_fstream.seekg(first_child_RRN * sha::HASH_LENGTH);
		hash_fstream.read(left_child, sha::HASH_LENGTH);

		//detect incomplete/missing child 1
		if(hash_fstream.eof()){
			bad_hash.first = first_child_RRN;
			bad_hash.second = first_child_RRN + 1;
			return true;
		}

		hash_fstream.read(right_child, sha::HASH_LENGTH);

		//detect incomplete/missing child 2
		if(hash_fstream.eof()){
			bad_hash.first = first_child_RRN + 1;
			bad_hash.second = first_child_RRN + 2;
			return true;
		}

		if(!check_hash(hash, left_child, right_child)){
			bad_hash.first = (2 * (current_RRN - start_RRN) + end_RRN + 1);
			bad_hash.second = (2 * (current_RRN - start_RRN) + end_RRN + 2);
			hash_fstream.close();
			return true;
		}

		++current_RRN;
	}

	hash_fstream.close();
}

std::string hash_tree::create_hash_tree(std::string file_name)
{
	//holds one file block
	char chunk[block_size];

	//null hash for comparison
	char empty[sha::HASH_LENGTH];
	memset(empty, '\0', sha::HASH_LENGTH);

	std::fstream fin(file_name.c_str(), std::ios::in | std::ios::binary);
	std::fstream scratch((global::HASH_DIRECTORY+"scratch").c_str(), std::ios::in
		| std::ios::out | std::ios::trunc | std::ios::binary);

	do{
		if(stop_thread){
			return "";
		}

		fin.read(chunk, block_size);
		SHA.init();
		SHA.load(chunk, fin.gcount());
		SHA.end();
		scratch.write(SHA.raw_hash(), sha::HASH_LENGTH);
	}while(fin.good());

	if(fin.bad() || !fin.eof()){
		logger::debug(LOGGER_P1,"error reading newly created hash tree file");
		assert(false);
	}

	std::string root_hash; //will contain the root hash in hex format
	create_hash_tree_recurse_found = false;

	//start recursive tree generating function
	create_hash_tree_recurse(scratch, 0, scratch.tellp(), root_hash);
	scratch.close();

	//clear the scratch file
	scratch.open((global::HASH_DIRECTORY+"scratch").c_str(), std::ios::trunc | std::ios::out);
	scratch.close();

	return root_hash;
}

void hash_tree::create_hash_tree_recurse(std::fstream & scratch, std::streampos row_start, std::streampos row_end, std::string & root_hash)
{
	//holds one hash
	char chunk[sha::HASH_LENGTH];

	//null hash for comparison
	char empty[sha::HASH_LENGTH];
	memset(empty, '\0', sha::HASH_LENGTH);

	//stopping case, if one hash passed in it's the root hash
	if(row_end - row_start == sha::HASH_LENGTH){
		return;
	}

	//if passed an odd amount of hashes append a NULL hash to inicate termination
	if(((row_end - row_start)/sha::HASH_LENGTH) % 2 != 0){
		scratch.seekp(row_end);
		scratch.write(empty, sha::HASH_LENGTH);
		row_end += sha::HASH_LENGTH;
	}

	//save position of last read and write with these, restore when switching
	std::streampos scratch_read, scratch_write;
	scratch_read = row_start;
	scratch_write = row_end;

	//loop through all hashes in the lower row and create the next highest row
	scratch.seekg(scratch_read);
	while(scratch_read != row_end){
		if(stop_thread){
			return;
		}

		//each new upper hash requires two lower hashes
		//read hash 1
		scratch.seekg(scratch_read);
		scratch.read(chunk, sha::HASH_LENGTH);
		scratch_read = scratch.tellg();
		SHA.init();
		SHA.load(chunk, scratch.gcount());

		//read hash 2
		scratch.read(chunk, sha::HASH_LENGTH);
		scratch_read = scratch.tellg();
		SHA.load(chunk, scratch.gcount());

		SHA.end();

		//write resulting hash
		scratch.seekp(scratch_write);
		scratch.write(SHA.raw_hash(), sha::HASH_LENGTH);
		scratch_write = scratch.tellp();
	}

	//recurse
	create_hash_tree_recurse(scratch, row_end, scratch_write, root_hash);

	if(create_hash_tree_recurse_found){
		return;
	}

	/*
	Writing of the result hash tree file is depth first, the root node will be
	added first.
	*/
	std::fstream fout;
	if(scratch_write - row_end == sha::HASH_LENGTH){
		root_hash = SHA.hex_hash();

		//do nothing if hash tree already exists
		std::fstream fin((global::HASH_DIRECTORY+root_hash).c_str(), std::ios::in);
		if(fin.is_open()){
			create_hash_tree_recurse_found = true;
			return;
		}

		//create the file with the root hash as name
		fout.open((global::HASH_DIRECTORY+root_hash).c_str(), std::ios::out | std::ios::app | std::ios::binary);
		fout.write(SHA.raw_hash(), sha::HASH_LENGTH);
	}else{
		//this is not the recursive call with the root hash
		fout.open((global::HASH_DIRECTORY+root_hash).c_str(), std::ios::out | std::ios::app | std::ios::binary);
	}

	//write hashes that were passed to this function
	scratch.seekg(row_start);
	while(scratch.tellg() != row_end){
		scratch.read(chunk, sha::HASH_LENGTH);
		fout.write(chunk, sha::HASH_LENGTH);
	}

	fout.close();

	return;
}

unsigned long hash_tree::locate_start(std::string root_hash)
{
	//null hash used to determine if a row is terminating early
	char end_of_row[sha::HASH_LENGTH];
	memset(end_of_row, '\0', sha::HASH_LENGTH);

	std::fstream fin((global::HASH_DIRECTORY+root_hash).c_str(), std::fstream::in);
	fin.seekg(0, std::ios::end);
	unsigned int max_possible_RRN = fin.tellg() / sha::HASH_LENGTH - 1;

	unsigned long current_RRN = 0;
	unsigned long start_RRN = 0;   //RRN of the start of the current row
	unsigned long end_RRN = 0;     //RRN of the end of the current row

	//holder for one hash
	char hash[sha::HASH_LENGTH];

	while(true){
		fin.seekg(current_RRN * sha::HASH_LENGTH);
		fin.read(hash, sha::HASH_LENGTH);

		if(memcmp(hash, end_of_row, sha::HASH_LENGTH) == 0){
			//row terminated early
			unsigned long start_RRN_temp = start_RRN;
			start_RRN = end_RRN + 1;
			end_RRN = 2 * (end_RRN - start_RRN_temp) + current_RRN;
			current_RRN = end_RRN;

			if(end_RRN == max_possible_RRN){
				fin.close();
				return start_RRN;
			}

			if(end_RRN > max_possible_RRN){
				logger::debug(LOGGER_P1,"corrupt hash tree");
				assert(false);
			}
		}else{
			unsigned long start_RRN_temp = start_RRN;
			start_RRN = end_RRN + 1;
			end_RRN = 2 * (end_RRN - start_RRN_temp) + current_RRN + 2;
			current_RRN = end_RRN;

			if(end_RRN == max_possible_RRN){
				fin.close();
				return start_RRN;
			}

			if(end_RRN > max_possible_RRN){
				logger::debug(LOGGER_P1,"corrupt hash tree");
				assert(false);
			}
		}
	}
}

void hash_tree::replace_hash(std::string root_hash, const unsigned long & number, char hash[])
{
	std::fstream hash_fstream((global::HASH_DIRECTORY+root_hash).c_str(), std::ios::out | std::ios::binary);
	hash_fstream.seekp(sha::HASH_LENGTH * number);
	hash_fstream.write(hash, sha::HASH_LENGTH);
}

void hash_tree::stop()
{
	stop_thread = true;
}
