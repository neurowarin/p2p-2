#include "message_tcp.hpp"

//BEGIN send::base
bool message_tcp::send::base::encrypt()
{
	return true;
}
//END send::base

//BEGIN recv::block
message_tcp::recv::block::block(
	handler func_in,
	const unsigned block_size_in,
	boost::shared_ptr<network::speed_calculator> Download_Speed_in
):
	func(func_in),
	block_size(block_size_in),
	bytes_seen(1)
{
	Download_Speed = Download_Speed_in;
}

bool message_tcp::recv::block::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	return recv_buf[0] == protocol::block;
}

message_tcp::recv::status message_tcp::recv::block::recv(network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	if(recv_buf.size() >= protocol::block_size(block_size)){
		network::buffer buf;
		Download_Speed->add(block_size - bytes_seen);
		buf.append(recv_buf.data() + 1, protocol::block_size(block_size) - 1);
		recv_buf.erase(0, protocol::block_size(block_size));
		if(func(buf)){
			return complete;
		}else{
			return blacklist;
		}
	}
	Download_Speed->add(recv_buf.size() - bytes_seen);
	bytes_seen = recv_buf.size();
	return incomplete;
}
//END recv::block

//BEGIN recv::close_slot
message_tcp::recv::close_slot::close_slot(
	handler func_in
):
	func(func_in)
{

}

bool message_tcp::recv::close_slot::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	return recv_buf[0] == protocol::close_slot;
}

message_tcp::recv::status message_tcp::recv::close_slot::recv(network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	if(recv_buf.size() >= protocol::close_slot_size){
		unsigned char slot_num = recv_buf[1];
		recv_buf.erase(0, protocol::close_slot_size);
		if(func(slot_num)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::close_slot

//BEGIN recv::composite
void message_tcp::recv::composite::add(boost::shared_ptr<base> M)
{
	assert(M);
	possible_response.push_back(M);
}

bool message_tcp::recv::composite::expect(network::buffer & recv_buf)
{
	for(std::vector<boost::shared_ptr<base> >::iterator
		iter_cur = possible_response.begin(), iter_end = possible_response.end();
		iter_cur != iter_end; ++iter_cur)
	{
		status Status = (*iter_cur)->recv(recv_buf);
		if((*iter_cur)->expect(recv_buf)){
			return true;
		}
	}
	return false;
}

message_tcp::recv::status message_tcp::recv::composite::recv(network::buffer & recv_buf)
{
	for(std::vector<boost::shared_ptr<base> >::iterator
		iter_cur = possible_response.begin(), iter_end = possible_response.end();
		iter_cur != iter_end; ++iter_cur)
	{
		status Status = (*iter_cur)->recv(recv_buf);
		if(Status != not_expected){
			return Status;
		}
	}
	return not_expected;
}
//END recv::composite

//BEGIN recv::error
message_tcp::recv::error::error(
	handler func_in
):
	func(func_in)
{

}

bool message_tcp::recv::error::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	return recv_buf[0] == protocol::error;
}

message_tcp::recv::status message_tcp::recv::error::recv(network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	if(recv_buf[0] == protocol::error){
		network::buffer buf;
		buf.append(recv_buf.data(), protocol::error_size);
		recv_buf.erase(0, protocol::error_size);
		if(func()){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::error

//BEGIN recv::have_file_block
message_tcp::recv::have_file_block::have_file_block(
	handler func_in,
	const unsigned char slot_num_in,
	const boost::uint64_t file_block_count_in
):
	func(func_in),
	slot_num(slot_num_in),
	file_block_count(file_block_count_in)
{

}

bool message_tcp::recv::have_file_block::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	if(recv_buf[0] != protocol::have_file_block){
		return false;
	}
	if(recv_buf.size() == 1){
		//don't have slot_num, assume we expect
		return true;
	}else{
		//recv_buf.size() >= 2
		return recv_buf[1] == slot_num;
	}
}

message_tcp::recv::status message_tcp::recv::have_file_block::recv(network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	unsigned expected_size = protocol::have_file_block_size(
		convert::VLI_size(file_block_count));
	if(recv_buf.size() >= expected_size){
		unsigned char slot_num = recv_buf[1];
		boost::uint64_t block_num = convert::bin_VLI_to_int(std::string(
		reinterpret_cast<char *>(recv_buf.data()) + 2, expected_size - 2));
		recv_buf.erase(0, expected_size);
		if(func(slot_num, block_num)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::have_file_block

//BEGIN recv::have_hash_tree_block
message_tcp::recv::have_hash_tree_block::have_hash_tree_block(
	handler func_in,
	const unsigned char slot_num_in,
	const boost::uint64_t tree_block_count_in
):
	func(func_in),
	slot_num(slot_num_in),
	tree_block_count(tree_block_count_in)
{

}

bool message_tcp::recv::have_hash_tree_block::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	if(recv_buf[0] != protocol::have_hash_tree_block){
		return false;
	}
	if(recv_buf.size() == 1){
		//don't have slot_num, assume we expect
		return true;
	}else{
		//recv_buf.size() >= 2
		return recv_buf[1] == slot_num;
	}
}

message_tcp::recv::status message_tcp::recv::have_hash_tree_block::recv(network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	unsigned expected_size = protocol::have_hash_tree_block_size(
		convert::VLI_size(tree_block_count));
	if(recv_buf.size() >= expected_size){
		unsigned char slot_num = recv_buf[1];
		boost::uint64_t block_num = convert::bin_VLI_to_int(std::string(
		reinterpret_cast<char *>(recv_buf.data()) + 2, expected_size - 2));
		recv_buf.erase(0, expected_size);
		if(func(slot_num, block_num)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::have_hash_tree_block

//BEGIN recv::initial
message_tcp::recv::initial::initial(
	handler func_in
):
	func(func_in)
{

}

bool message_tcp::recv::initial::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	//initial message random, no way to verify we expect
	return true;
}

message_tcp::recv::status message_tcp::recv::initial::recv(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	if(recv_buf.size() >= SHA1::bin_size){
		network::buffer buf;
		buf.append(recv_buf.data(), SHA1::bin_size);
		recv_buf.erase(0, SHA1::bin_size);
		if(func(buf)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::initial

//BEGIN recv::key_exchange_p_rA
message_tcp::recv::key_exchange_p_rA::key_exchange_p_rA(
	handler func_in
):
	func(func_in)
{

}

bool message_tcp::recv::key_exchange_p_rA::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	//no way to verify we expect
	return true;
}

message_tcp::recv::status message_tcp::recv::key_exchange_p_rA::recv(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	if(recv_buf.size() >= protocol::DH_key_size * 2){
		network::buffer buf;
		buf.append(recv_buf.data(), protocol::DH_key_size * 2);
		recv_buf.erase(0, protocol::DH_key_size * 2);
		if(func(buf)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::key_exchange_p_rA

//BEGIN recv::key_exchange_rB
message_tcp::recv::key_exchange_rB::key_exchange_rB(
	handler func_in
):
	func(func_in)
{

}

bool message_tcp::recv::key_exchange_rB::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	//no way to verify we expect
	return true;
}

message_tcp::recv::status message_tcp::recv::key_exchange_rB::recv(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	if(recv_buf.size() >= protocol::DH_key_size){
		network::buffer buf;
		buf.append(recv_buf.data(), protocol::DH_key_size);
		recv_buf.erase(0, protocol::DH_key_size);
		if(func(buf)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::key_exchange_rB

//BEGIN recv::request_hash_tree_block
message_tcp::recv::request_hash_tree_block::request_hash_tree_block(
	handler func_in,
	const unsigned char slot_num_in,
	const boost::uint64_t tree_block_count
):
	func(func_in),
	slot_num(slot_num_in),
	VLI_size(convert::VLI_size(tree_block_count))
{

}

bool message_tcp::recv::request_hash_tree_block::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	if(recv_buf.size() == 1){
		return recv_buf[0] == protocol::request_hash_tree_block;
	}else{
		return recv_buf[0] == protocol::request_hash_tree_block && recv_buf[1] == slot_num;
	}
}

message_tcp::recv::status message_tcp::recv::request_hash_tree_block::recv(
	network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	if(recv_buf.size() >= 2 + VLI_size){
		unsigned char slot_num = recv_buf[1];
		boost::uint64_t block_num = convert::bin_VLI_to_int(std::string(
			reinterpret_cast<char *>(recv_buf.data()) + 2, VLI_size));
		recv_buf.erase(0, 2 + VLI_size);
		if(func(slot_num, block_num)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::request_hash_tree_block

//BEGIN recv::request_file_block
message_tcp::recv::request_file_block::request_file_block(
	handler func_in,
	const unsigned char slot_num_in,
	const boost::uint64_t tree_block_count
):
	func(func_in),
	slot_num(slot_num_in),
	VLI_size(convert::VLI_size(tree_block_count))
{

}

bool message_tcp::recv::request_file_block::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	if(recv_buf.size() == 1){
		return recv_buf[0] == protocol::request_file_block;
	}else{
		return recv_buf[0] == protocol::request_file_block && recv_buf[1] == slot_num;
	}
}

message_tcp::recv::status message_tcp::recv::request_file_block::recv(network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	if(recv_buf.size() >= 2 + VLI_size){
		unsigned char slot_num = recv_buf[1];
		boost::uint64_t block_num = convert::bin_VLI_to_int(std::string(
			reinterpret_cast<char *>(recv_buf.data()) + 2, VLI_size));
		recv_buf.erase(0, protocol::request_file_block_size(VLI_size));
		if(func(slot_num, block_num)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::request_file_block

//BEGIN recv::request_slot
message_tcp::recv::request_slot::request_slot(
	handler func_in
):
	func(func_in)
{

}

bool message_tcp::recv::request_slot::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	return recv_buf[0] == protocol::request_slot;
}

message_tcp::recv::status message_tcp::recv::request_slot::recv(network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	if(recv_buf.size() >= protocol::request_slot_size){
		std::string hash = convert::bin_to_hex(std::string(
			reinterpret_cast<const char *>(recv_buf.data()) + 1, SHA1::bin_size));
		recv_buf.erase(0, protocol::request_slot_size);
		if(func(hash)){
			return complete;
		}else{
			return blacklist;
		}
	}
	return incomplete;
}
//END recv::request_slot

//BEGIN recv::slot
message_tcp::recv::slot::slot(
	handler func_in,
	const std::string & hash_in
):
	func(func_in),
	hash(hash_in),
	checked(false)
{

}

bool message_tcp::recv::slot::expect(network::buffer & recv_buf)
{
	assert(!recv_buf.empty());
	return recv_buf[0] == protocol::slot;
}

message_tcp::recv::status message_tcp::recv::slot::recv(network::buffer & recv_buf)
{
	if(!expect(recv_buf)){
		return not_expected;
	}
	if(recv_buf.size() < protocol::slot_size(0, 0)){
		//do not have the minimum size slot message
		return incomplete;
	}
	if(!checked){
		SHA1 SHA;
		SHA.init();
		SHA.load(reinterpret_cast<char *>(recv_buf.data()) + 3,
			8 + SHA1::bin_size);
		SHA.end();
		if(SHA.hex() != hash){
			return blacklist;
		}
	}
	unsigned char slot_num = recv_buf[1];
	boost::uint64_t file_size = convert::bin_to_int<boost::uint64_t>(
		std::string(reinterpret_cast<char *>(recv_buf.data()) + 3, 8));
	std::string root_hash = convert::bin_to_hex(std::string(
		reinterpret_cast<char *>(recv_buf.data()+11), SHA1::bin_size));
	bit_field tree_BF, file_BF;
	if(recv_buf[2] == 0){
		//no bit_field
		recv_buf.erase(0, protocol::slot_size(0, 0));
	}else if(recv_buf[2] == 1){
		//file bit_field
		boost::uint64_t file_block_count = file::calc_file_block_count(file_size);
		boost::uint64_t file_BF_size = bit_field::size_bytes(file_block_count);
		if(recv_buf.size() < protocol::slot_size(0, file_BF_size)){
			return incomplete;
		}
		file_BF.set_buf(recv_buf.data() + 31, file_BF_size, file_block_count);
		recv_buf.erase(0, protocol::slot_size(0, file_BF_size));
	}else if(recv_buf[2] == 2){
		//tree bit_field
		boost::uint64_t tree_block_count = hash_tree::calc_tree_block_count(file_size);
		boost::uint64_t tree_BF_size = bit_field::size_bytes(tree_block_count);
		if(recv_buf.size() < protocol::slot_size(tree_BF_size, 0)){
			return incomplete;
		}
		tree_BF.set_buf(recv_buf.data() + 31, tree_BF_size, tree_block_count);
		recv_buf.erase(0, protocol::slot_size(tree_BF_size, 0));
	}else if(recv_buf[2] == 3){
		//tree and file bit_field
		boost::uint64_t tree_block_count = hash_tree::calc_tree_block_count(file_size);
		boost::uint64_t file_block_count = file::calc_file_block_count(file_size);
		boost::uint64_t tree_BF_size = bit_field::size_bytes(tree_block_count);
		boost::uint64_t file_BF_size = bit_field::size_bytes(file_block_count);
		if(recv_buf.size() < protocol::slot_size(tree_BF_size, file_BF_size)){
			return incomplete;
		}
		tree_BF.set_buf(recv_buf.data() + 31, tree_BF_size, tree_block_count);
		file_BF.set_buf(recv_buf.data() + 31 + tree_BF_size, file_BF_size, file_block_count);
		recv_buf.erase(0, protocol::slot_size(tree_BF_size, file_BF_size));
	}else{
		LOGGER << "invalid status byte";
		return blacklist;
	}
	if(func(slot_num, file_size, root_hash, tree_BF, file_BF)){
		return complete;
	}else{
		return blacklist;
	}
}
//END recv::slot

//BEGIN send::block
message_tcp::send::block::block(network::buffer & block,
	boost::shared_ptr<network::speed_calculator> Upload_Speed_in)
{
	buf.append(protocol::block).append(block);
	Upload_Speed = Upload_Speed_in;
}
//END send::block

//BEGIN send::close_slot
message_tcp::send::close_slot::close_slot(const unsigned char slot_num)
{
	buf.append(protocol::close_slot).append(slot_num);
}
//END send::close_slot

//BEGIN send::error
message_tcp::send::error::error()
{
	buf.append(protocol::error);
}
//END send::error

//BEGIN send::have_file_block
message_tcp::send::have_file_block::have_file_block(
	const unsigned char slot_num,
	const boost::uint64_t block_num,
	const boost::uint64_t file_block_count
)
{
	buf.append(protocol::have_file_block)
		.append(slot_num)
		.append(convert::int_to_bin_VLI(block_num, file_block_count));
}
//END send::have_file_block

//BEGIN send::have_hash_tree_block
message_tcp::send::have_hash_tree_block::have_hash_tree_block(
	const unsigned char slot_num,
	const boost::uint64_t block_num,
	const boost::uint64_t tree_block_count
)
{
	buf.append(protocol::have_hash_tree_block)
		.append(slot_num)
		.append(convert::int_to_bin_VLI(block_num, tree_block_count));
}
//END send::have_hash_tree_block

//BEGIN send::initial
message_tcp::send::initial::initial(const std::string & ID)
{
	assert(ID.size() == SHA1::hex_size);
	buf = convert::hex_to_bin(ID);
}
//END send::initial

//BEGIN send::key_exchange_p_rA
message_tcp::send::key_exchange_p_rA::key_exchange_p_rA(encryption & Encryption)
{
	buf = Encryption.send_p_rA();
}

bool message_tcp::send::key_exchange_p_rA::encrypt()
{
	return false;
}
//END send::key_exchange_p_rA

//BEGIN send::key_exchange_rB
message_tcp::send::key_exchange_rB::key_exchange_rB(encryption & Encryption)
{
	buf = Encryption.send_rB();
}

bool message_tcp::send::key_exchange_rB::encrypt()
{
	return false;
}
//END send::key_exchange_rB

//BEGIN send::request_hash_tree_block
message_tcp::send::request_hash_tree_block::request_hash_tree_block(
	const unsigned char slot_num,
	const boost::uint64_t block_num,
	const boost::uint64_t tree_block_count
)
{
	buf.append(protocol::request_hash_tree_block)
		.append(slot_num)
		.append(convert::int_to_bin_VLI(block_num, tree_block_count));
}
//END send::request_hash_tree_block

//BEGIN send::request_file_block
message_tcp::send::request_file_block::request_file_block(
	const unsigned char slot_num,
	const boost::uint64_t block_num,
	const boost::uint64_t file_block_count)
{
	buf.append(protocol::request_file_block)
		.append(slot_num)
		.append(convert::int_to_bin_VLI(block_num, file_block_count));
}
//END send::request_file_block

//BEGIN send::request_slot
message_tcp::send::request_slot::request_slot(const std::string & hash)
{
	assert(hash.size() == SHA1::hex_size);
	buf.append(protocol::request_slot).append(convert::hex_to_bin(hash));
}
//END send::request_slot

//BEGIN send::slot
message_tcp::send::slot::slot(const unsigned char slot_num,
	const boost::uint64_t file_size, const std::string & root_hash,
	const bit_field & tree_BF, const bit_field & file_BF)
{
	unsigned char status;
	if(tree_BF.empty() && file_BF.empty()){
		status = 0;
	}else if(tree_BF.empty() && !file_BF.empty()){
		status = 1;
	}else if(!tree_BF.empty() && file_BF.empty()){
		status = 2;
	}else if(!tree_BF.empty() && !file_BF.empty()){
		status = 3;
	}
	buf.append(protocol::slot)
		.append(slot_num)
		.append(status)
		.append(convert::int_to_bin(file_size))
		.append(convert::hex_to_bin(root_hash));
	if(!tree_BF.empty()){
		buf.append(tree_BF.get_buf());
	}
	if(!file_BF.empty()){
		buf.append(file_BF.get_buf());
	}
}
//END send::slot
