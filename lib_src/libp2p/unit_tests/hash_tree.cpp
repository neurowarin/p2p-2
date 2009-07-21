//custom
#include "../database.hpp"
#include "../hash_tree.hpp"
#include "../path.hpp"
#include "../protocol.hpp"

//include
#include <logger.hpp>

//create test files to hash if they don't already exist
void create_test_file(const std::string & name, const unsigned & bytes)
{
	std::fstream f((path::share() + name).c_str(), std::ios::in);
	if(f.is_open()){
		return;
	}

	f.open((path::share() + name).c_str(), std::ios::out | std::ios::binary);
	f.clear();
	for(int x=0; x<bytes; ++x){
		if(x % 80 == 0 && x != 0){
			f.put('\n');
		}else{
			f.put('0');
		}
	}
	f.flush();
}

int main()
{
	//setup database and make sure hash table clear
	database::pool::singleton().unit_test_override("hash_tree.db");
	database::init init;
	database::table::hash::clear();

	//create required directories for the shared files and temp files
	path::create_required_directories();

	//create test files to hash
	create_test_file("1_block", 1 * protocol::FILE_BLOCK_SIZE);
	create_test_file("2_block", 2 * protocol::FILE_BLOCK_SIZE);
	create_test_file("3_block", 3 * protocol::FILE_BLOCK_SIZE);
	create_test_file("4_block", 4 * protocol::FILE_BLOCK_SIZE);
	create_test_file("256_block", 256 * protocol::FILE_BLOCK_SIZE);
	create_test_file("257_block", 257 * protocol::FILE_BLOCK_SIZE);

	//object which creates hash tree
	hash_tree Hash_Tree;

	//test 1
	std::string root_hash;
	boost::uint64_t bad_block;
	if(!Hash_Tree.create(path::share() + "1_block", 1 * protocol::FILE_BLOCK_SIZE, root_hash)){
		LOGGER; exit(1);
	}
	hash_tree::tree_info TE_1(root_hash, 1 * protocol::FILE_BLOCK_SIZE);
	if(Hash_Tree.check(TE_1, bad_block) != hash_tree::GOOD){
		LOGGER; exit(1);
	}

	//test 2
	if(!Hash_Tree.create(path::share() + "2_block", 2 * protocol::FILE_BLOCK_SIZE, root_hash)){
		LOGGER; exit(1);
	}
	hash_tree::tree_info TE_2(root_hash, 2 * protocol::FILE_BLOCK_SIZE);
	if(Hash_Tree.check(TE_2, bad_block) != hash_tree::GOOD){
		LOGGER; exit(1);
	}

	//test 3
	if(!Hash_Tree.create(path::share() + "3_block", 3 * protocol::FILE_BLOCK_SIZE, root_hash)){
		LOGGER; exit(1);
	}
	hash_tree::tree_info TE_3(root_hash, 3 * protocol::FILE_BLOCK_SIZE);
	if(Hash_Tree.check(TE_3, bad_block) != hash_tree::GOOD){
		LOGGER; exit(1);
	}

	//test 4
	if(!Hash_Tree.create(path::share() + "4_block", 4 * protocol::FILE_BLOCK_SIZE, root_hash)){
		LOGGER; exit(1);
	}
	hash_tree::tree_info TE_4(root_hash, 4 * protocol::FILE_BLOCK_SIZE);
	if(Hash_Tree.check(TE_4, bad_block) != hash_tree::GOOD){
		LOGGER; exit(1);
	}

	//test 5
	if(!Hash_Tree.create(path::share() + "256_block", 256 * protocol::FILE_BLOCK_SIZE, root_hash)){
		LOGGER; exit(1);
	}
	hash_tree::tree_info TE_256(root_hash, 256 * protocol::FILE_BLOCK_SIZE);
	if(Hash_Tree.check(TE_256, bad_block) != hash_tree::GOOD){
		LOGGER; exit(1);
	}

	//test 6
	if(!Hash_Tree.create(path::share() + "257_block", 257 * protocol::FILE_BLOCK_SIZE, root_hash)){
		LOGGER; exit(1);
	}
	hash_tree::tree_info TE_257(root_hash, 257 * protocol::FILE_BLOCK_SIZE);
	if(Hash_Tree.check(TE_257, bad_block) != hash_tree::GOOD){
		LOGGER; exit(1);
	}
}
