#ifndef H_MESSAGE
#define H_MESSAGE

//custom
#include "encryption.hpp"
#include "file.hpp"
#include "hash_tree.hpp"
#include "protocol.hpp"

//include
#include <bit_field.hpp>
#include <boost/shared_ptr.hpp>
#include <convert.hpp>
#include <network/network.hpp>

//standard
#include <list>
#include <map>

namespace message{

//abstract base class for all messages
class base
{
public:
	//function to handle incoming message (empty if message to send)
	boost::function<bool (boost::shared_ptr<base>)> func;

	//bytes that have been sent or received
	network::buffer buf;

	//speed calculator for upload or download (empty if no speed to track)
	boost::shared_ptr<network::speed_calculator> Speed_Calculator;

	/*
	encrypt:
		Returns true if message should be encrypted before sending. The default is
		true. The key_exchange messages override this and return false.
	expects:
		Returns true if recv() expects message on front of CI.recv_buf.
	recv:
		Returns true if incoming message received. False if incomplete message or
		host blacklisted.
	*/
	virtual bool encrypt();
	virtual bool expects(network::buffer & recv_buf) = 0;
	virtual bool recv(network::connection_info & CI) = 0;
};

class block : public base
{
public:
	//recv ctor
	block(boost::function<bool (boost::shared_ptr<base>)> func_in,
		const boost::uint64_t block_size_in,
		boost::shared_ptr<network::speed_calculator> Download_Speed);
	//send ctor
	explicit block(network::buffer & block,
		boost::shared_ptr<network::speed_calculator> Upload_Speed);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
private:
	//size of block field (only used for recv)
	boost::uint64_t block_size;

	//how many bytes have already been added to download speed
	unsigned bytes_seen;
};

class close_slot : public base
{
public:
	//recv ctor
	explicit close_slot(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//send ctor
	close_slot(const unsigned char slot_num);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
};

//the composite message is used to handle multiple possible responses
class composite : public base
{
public:
	//add messages to expect() and recv()
	void add(boost::shared_ptr<base> M);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
private:
	boost::shared_ptr<base> response;
	std::vector<boost::shared_ptr<base> > possible_response;
	//dummy that func is initially set to
	bool dummy(boost::shared_ptr<base> M);
};

class error : public base
{
public:
	//recv ctor
	explicit error(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//send ctor
	error();
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
};

class have_file_block : public base
{
public:
	//ctor to recv message
	have_file_block(boost::function<bool (boost::shared_ptr<base>)> func_in,
		const unsigned char slot_num_in, const boost::uint64_t file_block_count_in);
	//ctor to send message
	have_file_block(const unsigned char slot_num_in,
		const boost::uint64_t block_num, const boost::uint64_t file_block_count_in);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
private:
	const unsigned char slot_num;

	//size (bytes) of block number VLI, only used for recv
	const boost::uint64_t file_block_count;
};

class have_hash_tree_block : public base
{
public:
	//ctor to recv message
	have_hash_tree_block(boost::function<bool (boost::shared_ptr<base>)> func_in,
		const unsigned char slot_num_in, const boost::uint64_t tree_block_count_in);
	//ctor to send message
	have_hash_tree_block(const unsigned char slot_num_in,
		const boost::uint64_t block_num, const boost::uint64_t tree_block_count_in);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
private:
	const unsigned char slot_num;

	//size (bytes) of block number VLI, only used for recv
	const boost::uint64_t tree_block_count;
};

class initial : public base
{
public:
	//ctor to recv message
	explicit initial(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//ctor to send message
	explicit initial(const std::string peer_ID);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
};

class key_exchange_p_rA : public base
{
public:
	//recv ctor
	explicit key_exchange_p_rA(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//send ctor
	explicit key_exchange_p_rA(encryption & Encryption);
	virtual bool encrypt();
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
};

class key_exchange_rB : public base
{
public:
	//recv ctor
	explicit key_exchange_rB(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//send ctor
	explicit key_exchange_rB(encryption & Encryption);
	virtual bool encrypt();
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
};

class request_hash_tree_block : public base
{
public:
	//recv ctor
	request_hash_tree_block(boost::function<bool (boost::shared_ptr<base>)> func_in,
		const unsigned char slot_num_in, const boost::uint64_t tree_block_count);
	//send ctor
	request_hash_tree_block(const unsigned char slot_num,
		const boost::uint64_t block_num, const boost::uint64_t tree_block_count);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
private:
	//these are only initialized by recv ctor
	unsigned char slot_num;
	unsigned VLI_size; //size (bytes) of the block number field
};

class request_file_block : public base
{
public:
	//recv ctor
	request_file_block(boost::function<bool (boost::shared_ptr<base>)> func_in,
		const unsigned char slot_num_in, const boost::uint64_t file_block_count);
	//send ctor
	request_file_block(const unsigned char slot_num,
		const boost::uint64_t block_num, const boost::uint64_t file_block_count);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
private:
	//these are only initialized by recv ctor
	unsigned char slot_num;
	unsigned VLI_size; //size (bytes) of the block number field
};

class request_slot : public base
{
public:
	//recv ctor
	explicit request_slot(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//send ctor
	explicit request_slot(const std::string & hash);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
};

class slot : public base
{
public:
	//recv ctor
	slot(boost::function<bool (boost::shared_ptr<base>)> func_in,
		const std::string & hash_in);
	//send ctor
	slot(const unsigned char slot_num, const boost::uint64_t file_size,
		const std::string & root_hash, const bit_field & tree_BF,
		const bit_field & file_BF);
	virtual bool expects(network::buffer & recv_buf);
	virtual bool recv(network::connection_info & CI);
private:
	//hash requested (only used for recv)
	std::string hash;
	/*
	Set to true when file_size and root_hash checked. This is used to save
	processing from computing the hash multiple times. (only used for recv)
	*/
	bool checked;
};
}//end namespace message
#endif
