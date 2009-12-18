#ifndef H_PROTOCOL
#define H_PROTOCOL

//include
#include <SHA1.hpp>

namespace protocol
{
//hard coded protocol preferences
const unsigned timeout = 16;          //time after which a block will be rerequested
const unsigned DH_key_size = 16;      //size exchanged key in Diffie-Hellman-Merkle
const unsigned hash_block_size = 512; //number of hashes in hash block
const unsigned file_block_size = hash_block_size * SHA1::bin_size;

//commands and messages sizes
const unsigned char error = static_cast<unsigned char>(0);
const unsigned error_size = 1;
const unsigned char request_slot = static_cast<unsigned char>(1);
const unsigned request_slot_size = 21;
const unsigned char slot = static_cast<unsigned char>(2);
static const unsigned slot_size(const unsigned bit_field_1_size,
	const unsigned bit_field_2_size)
{
	return 31 + bit_field_1_size + bit_field_2_size;
}
const unsigned char request_hash_tree_block = static_cast<unsigned char>(3);
static unsigned request_hash_tree_block_size(const unsigned VLI_size)
{
	return 2 + VLI_size;
}
const unsigned char request_file_block = static_cast<unsigned char>(4);
static unsigned request_file_block_size(const unsigned VLI_size)
{
	return 2 + VLI_size;
}
const unsigned char block = static_cast<unsigned char>(5);
static unsigned block_size(const unsigned size)
{
	return 1 + size;
}
const unsigned char have_hash_tree_block = static_cast<unsigned char>(6);
static unsigned have_hash_tree_block_size(const unsigned VLI_size)
{
	return 2 + VLI_size;
}
const unsigned char have_file_block = static_cast<unsigned char>(7);
static unsigned have_file_block_size(const unsigned VLI_size)
{
	return 2 + VLI_size;
}
const unsigned char close_slot = static_cast<unsigned char>(8);
const unsigned close_slot_size = 2;
}//end of protocol namespace
#endif
