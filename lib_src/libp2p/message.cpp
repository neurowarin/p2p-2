#include "message.hpp"

//BEGIN base
bool message::base::encrypt()
{
	return true;
}
//END base

//BEGIN close_slot
message::close_slot::close_slot(boost::function<bool (boost::shared_ptr<base>)> func_in)
{
	func = func_in;
}

message::close_slot::close_slot(const unsigned char slot_num)
{
	buf.append(protocol::close_slot).append(slot_num);
}

bool message::close_slot::expects(network::buffer & recv_buf)
{
	return recv_buf[0] == protocol::close_slot;
}

bool message::close_slot::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf.size() >= protocol::close_slot_size){
		buf.append(CI.recv_buf.data(), protocol::close_slot_size);
		CI.recv_buf.erase(0, protocol::close_slot_size);
		return true;
	}
	return false;
}
//END close_slot

//BEGIN composite
void message::composite::add(boost::shared_ptr<base> M)
{
	assert(M);
	assert(M->func);

	/*
	We set func to a dummy handler to pass asserts in the connection class which
	make sure a handler is defined. This makes sure that at least one message is
	added to the composite message. This will be set later, by recv, to the
	correct handler.
	*/
	func = boost::bind(&composite::dummy, this, _1);

	possible_response.push_back(M);
}

bool message::composite::dummy(boost::shared_ptr<base> M)
{
	LOGGER << "programmer error: improperly used composite message";
	exit(1);
}

bool message::composite::expects(network::buffer & recv_buf)
{
	for(std::vector<boost::shared_ptr<base> >::iterator
		iter_cur = possible_response.begin(), iter_end = possible_response.end();
		iter_cur != iter_end; ++iter_cur)
	{
		if((*iter_cur)->expects(recv_buf)){
			return true;
		}
	}
	return false;
}

bool message::composite::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	for(std::vector<boost::shared_ptr<base> >::iterator
		iter_cur = possible_response.begin(), iter_end = possible_response.end();
		iter_cur != iter_end; ++iter_cur)
	{
		if((*iter_cur)->expects(CI.recv_buf)){
			if((*iter_cur)->recv(CI)){
				func = (*iter_cur)->func;
				buf.clear();
				buf.swap((*iter_cur)->buf);
				response = *iter_cur;
				return true;
			}
			return false;
		}
	}
}
//END composite

//BEGIN block
message::block::block(
	boost::function<bool (boost::shared_ptr<base>)> func_in,
	const boost::uint64_t block_size_in,
	boost::shared_ptr<network::speed_calculator> Download_Speed
):
	block_size(block_size_in),
	bytes_seen(0)
{
	func = func_in;
	Speed_Calculator = Download_Speed;
}

message::block::block(network::buffer & block,
	boost::shared_ptr<network::speed_calculator> Upload_Speed)
{
	buf.append(protocol::block).append(block);
	Speed_Calculator = Upload_Speed;
}

bool message::block::expects(network::buffer & recv_buf)
{
	return recv_buf[0] == protocol::block;
}

bool message::block::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	Speed_Calculator->add(CI.recv_buf.size() - bytes_seen);
	bytes_seen = CI.recv_buf.size();
	if(CI.recv_buf.size() >= protocol::block_size(block_size)){
		buf.append(CI.recv_buf.data(), protocol::block_size(block_size));
		CI.recv_buf.erase(0, protocol::block_size(block_size));
		return true;
	}
	return false;
}
//END block

//BEGIN error
message::error::error(boost::function<bool (boost::shared_ptr<base>)> func_in)
{
	func = func_in;
}

message::error::error()
{
	buf.append(protocol::error);
}

bool message::error::expects(network::buffer & recv_buf)
{
	return recv_buf[0] == protocol::error;
}

bool message::error::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf[0] == protocol::error){
		buf.append(CI.recv_buf.data(), protocol::error_size);
		CI.recv_buf.erase(0, protocol::error_size);
		return true;
	}
	return false;
}
//END error

//BEGIN initial
message::initial::initial(boost::function<bool (boost::shared_ptr<base>)> func_in)
{
	func = func_in;
}

message::initial::initial(const std::string peer_ID)
{
	buf = convert::hex_to_bin(peer_ID);
}

bool message::initial::expects(network::buffer & recv_buf)
{
	//initial doesn't begin with any command
	return true;
}

bool message::initial::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf.size() >= SHA1::bin_size){
		buf.append(CI.recv_buf.data(), SHA1::bin_size);
		CI.recv_buf.erase(0, SHA1::bin_size);
		return true;
	}
	return false;
}
//END initial

//BEGIN key_exchange_p_rA
message::key_exchange_p_rA::key_exchange_p_rA(
	boost::function<bool (boost::shared_ptr<base>)> func_in)
{
	func = func_in;
}

message::key_exchange_p_rA::key_exchange_p_rA(encryption & Encryption)
{
	buf = Encryption.send_p_rA();
}

bool message::key_exchange_p_rA::encrypt()
{
	return false;
}

bool message::key_exchange_p_rA::expects(network::buffer & recv_buf)
{
	//p rA doesn't begin with any command
	return true;
}

bool message::key_exchange_p_rA::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf.size() >= protocol::DH_key_size * 2){
		buf.append(CI.recv_buf.data(), protocol::DH_key_size * 2);
		CI.recv_buf.erase(0, protocol::DH_key_size * 2);
		return true;
	}
	return false;
}
//END key_exchange_p_rA

//BEGIN key_exchange_rB
message::key_exchange_rB::key_exchange_rB(
	boost::function<bool (boost::shared_ptr<base>)> func_in)
{
	func = func_in;
}

message::key_exchange_rB::key_exchange_rB(encryption & Encryption)
{
	buf = Encryption.send_rB();
}

bool message::key_exchange_rB::encrypt()
{
	return false;
}

bool message::key_exchange_rB::expects(network::buffer & recv_buf)
{
	//rB doesn't begin with any command
	return true;
}

bool message::key_exchange_rB::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf.size() >= protocol::DH_key_size){
		buf.append(CI.recv_buf.data(), protocol::DH_key_size);
		CI.recv_buf.erase(0, protocol::DH_key_size);
		return true;
	}
	return false;
}
//END key_exchange_rB

//BEGIN request_hash_tree_block
message::request_hash_tree_block::request_hash_tree_block(
	boost::function<bool (boost::shared_ptr<base>)> func_in,
	const unsigned char slot_num_in,
	const boost::uint64_t tree_block_count
):
	slot_num(slot_num_in),
	VLI_size(convert::VLI_size(tree_block_count))
{
	func = func_in;
}


message::request_hash_tree_block::request_hash_tree_block(
	const unsigned char slot_num,
	const boost::uint64_t block_num,
	const boost::uint64_t tree_block_count
)
{
	buf.append(protocol::request_hash_tree_block)
		.append(slot_num)
		.append(convert::encode_VLI(block_num, tree_block_count));
}

bool message::request_hash_tree_block::expects(network::buffer & recv_buf)
{
	if(recv_buf.size() == 1 && recv_buf[0] == protocol::request_hash_tree_block){
		/*
		We don't have the slot number yet so we can't be sure if we expect this
		message or not. Return true and wait for the next byte.
		*/
		return true;
	}else if(recv_buf.size() > 1 && recv_buf[0] == protocol::request_hash_tree_block
		&& recv_buf[1] == slot_num)
	{
		return true;
	}
	return false;
}

bool message::request_hash_tree_block::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf.size() >= 2 + VLI_size){
		buf.append(CI.recv_buf.data(), 2 + VLI_size);
		CI.recv_buf.erase(0, 2 + VLI_size);
		return true;
	}
	return false;
}
//END request_hash_tree_block

//BEGIN request_file_block
message::request_file_block::request_file_block(
	boost::function<bool (boost::shared_ptr<base>)> func_in,
	const unsigned char slot_num_in,
	const boost::uint64_t tree_block_count
):
	slot_num(slot_num_in),
	VLI_size(convert::VLI_size(tree_block_count))
{
	func = func_in;
}

message::request_file_block::request_file_block(
	const unsigned char slot_num,
	const boost::uint64_t block_num,
	const boost::uint64_t file_block_count)
{
	buf.append(protocol::request_file_block)
		.append(slot_num)
		.append(convert::encode_VLI(block_num, file_block_count));
}

bool message::request_file_block::expects(network::buffer & recv_buf)
{
	if(recv_buf.size() == 1 && recv_buf[0] == protocol::request_file_block){
		/*
		We don't have the slot number yet so we can't be sure if we expect this
		message or not. Return true and wait for the next byte.
		*/
		return true;
	}else if(recv_buf.size() > 1 && recv_buf[0] == protocol::request_file_block
		&& recv_buf[1] == slot_num)
	{
		return true;
	}
	return false;
}

bool message::request_file_block::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf.size() >= 2 + VLI_size){
		buf.append(CI.recv_buf.data(), 2 + VLI_size);
		CI.recv_buf.erase(0, 2 + VLI_size);
		return true;
	}
	return false;
}
//END request_file_block

//BEGIN request_slot
message::request_slot::request_slot(
	boost::function<bool (boost::shared_ptr<base>)> func_in)
{
	func = func_in;
}

message::request_slot::request_slot(const std::string & hash)
{
	buf.append(protocol::request_slot).append(convert::hex_to_bin(hash));
}

bool message::request_slot::expects(network::buffer & recv_buf)
{
	return recv_buf[0] == protocol::request_slot;
}

bool message::request_slot::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf.size() >= protocol::request_slot_size){
		buf.append(CI.recv_buf.data(), protocol::request_slot_size);
		CI.recv_buf.erase(0, protocol::request_slot_size);
		return true;
	}
	return false;
}
//END request_slot

//BEGIN slot
message::slot::slot(
	boost::function<bool (boost::shared_ptr<base>)> func_in,
	const std::string & hash_in
):
	hash(hash_in),
	checked(false)
{
	func = func_in;
}

message::slot::slot(const unsigned char slot_num, unsigned char status,
	const boost::uint64_t file_size, const std::string & root_hash)
{
	buf.append(protocol::slot)
		.append(slot_num)
		.append(status)
		.append(convert::encode(file_size))
		.append(convert::hex_to_bin(root_hash));
}

bool message::slot::expects(network::buffer & recv_buf)
{
	return recv_buf[0] == protocol::slot;
}

bool message::slot::recv(network::connection_info & CI)
{
	assert(expects(CI.recv_buf));
	if(CI.recv_buf.size() < protocol::slot_size(0, 0)){
		//we do not have the minimum size slot message
		return false;
	}
	if(!checked){
		SHA1 SHA;
		SHA.init();
		SHA.load(reinterpret_cast<char *>(CI.recv_buf.data()) + 3,
			8 + SHA1::bin_size);
		SHA.end();
		if(SHA.hex() != hash){
			LOGGER << "invalid slot message from " << CI.IP;
			database::table::blacklist::add(CI.IP);
			return false;
		}
	}
	if(CI.recv_buf[2] == static_cast<unsigned char>(0)){
		//no bitfields to follow
		buf.append(CI.recv_buf.data(), protocol::slot_size(0, 0));
		CI.recv_buf.erase(0, protocol::slot_size(0, 0));
		return true;
	}else{
		LOGGER << "stub: need to process slot message where k != 0";
		exit(1);
	}
	return true;
}
//END slot
