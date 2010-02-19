#ifndef H_MESSAGE_TCP
#define H_MESSAGE_TCP

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

namespace message_tcp{

enum status{
	blacklist,           //host needs to be blacklisted
	expected_incomplete, //incomplete message on front of buf
	expected_complete,   //received complete messages (stored in base::buf)
	not_expected         //messages doesn't expect bytes on front of buf
};

//abstract base class for all messages
class base
{
public:
	//bytes that have been sent or received
	network::buffer buf;

	//speed calculator for upload or download (empty if no speed to track)
	boost::shared_ptr<network::speed_calculator> Speed_Calculator;

	/*
	encrypt:
		Returns true if message should be encrypted before sending. The default is
		true. The key_exchange messages override this and return false.
	expect:
		Returnst true if we expect message on front of recv_buf.
	recv:
		Tries to parse message on front of buffer. Returns a status.
		Note: It is not necessary to call expect before this.
	*/
	virtual bool encrypt();
	virtual bool expect(network::buffer & recv_buf) = 0;
	virtual status recv(network::buffer & recv_buf) = 0;

protected:
	//function to handle incoming message (empty if message for send)
	boost::function<bool (network::buffer &)> func;
};

class block : public base
{
public:
	//recv ctor
	block(boost::function<bool (network::buffer &)> func_in,
		const unsigned block_size_in,
		boost::shared_ptr<network::speed_calculator> Download_Speed);
	//send ctor
	explicit block(network::buffer & block,
		boost::shared_ptr<network::speed_calculator> Upload_Speed);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
private:
	unsigned block_size; //size of block field (used only for recv)
	unsigned bytes_seen; //how many bytes of message have already been seen
};

class close_slot : public base
{
public:
	//recv ctor
	explicit close_slot(boost::function<bool (network::buffer &)> func_in);
	//send ctor
	close_slot(const unsigned char slot_num);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
};

//the composite message is used to handle multiple possible responses
class composite : public base
{
public:
	//add messages to expect() and recv()
	void add(boost::shared_ptr<base> M);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
private:
	std::vector<boost::shared_ptr<base> > possible_response;
};

class error : public base
{
public:
	//recv ctor
	explicit error(boost::function<bool (network::buffer &)> func_in);
	//send ctor
	error();
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
};

class have_file_block : public base
{
public:
	//ctor to recv message
	have_file_block(boost::function<bool (network::buffer &)> func_in,
		const unsigned char slot_num_in, const boost::uint64_t file_block_count_in);
	//ctor to send message
	have_file_block(const unsigned char slot_num_in,
		const boost::uint64_t block_num, const boost::uint64_t file_block_count_in);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
private:
	const unsigned char slot_num;           //used only for recv
	const boost::uint64_t file_block_count; //used only for recv
};

class have_hash_tree_block : public base
{
public:
	//ctor to recv message
	have_hash_tree_block(boost::function<bool (network::buffer &)> func_in,
		const unsigned char slot_num_in, const boost::uint64_t tree_block_count_in);
	//ctor to send message
	have_hash_tree_block(const unsigned char slot_num_in,
		const boost::uint64_t block_num, const boost::uint64_t tree_block_count_in);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
private:
	const unsigned char slot_num;           //used only for recv
	const boost::uint64_t tree_block_count; //used only for recv
};

class initial : public base
{
public:
	//ctor to recv message
	explicit initial(boost::function<bool (network::buffer &)> func_in);
	//ctor to send message
	explicit initial(const std::string ID);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
};

class key_exchange_p_rA : public base
{
public:
	//recv ctor
	explicit key_exchange_p_rA(boost::function<bool (network::buffer &)> func_in);
	//send ctor
	explicit key_exchange_p_rA(encryption & Encryption);
	virtual bool encrypt();
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
};

class key_exchange_rB : public base
{
public:
	//recv ctor
	explicit key_exchange_rB(boost::function<bool (network::buffer &)> func_in);
	//send ctor
	explicit key_exchange_rB(encryption & Encryption);
	virtual bool encrypt();
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
};

class request_hash_tree_block : public base
{
public:
	//recv ctor
	request_hash_tree_block(boost::function<bool (network::buffer &)> func_in,
		const unsigned char slot_num_in, const boost::uint64_t tree_block_count);
	//send ctor
	request_hash_tree_block(const unsigned char slot_num,
		const boost::uint64_t block_num, const boost::uint64_t tree_block_count);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
private:
	unsigned char slot_num; //used only for recv
	unsigned VLI_size;      //block number field size (bytes), used only for recv
};

class request_file_block : public base
{
public:
	//recv ctor
	request_file_block(boost::function<bool (network::buffer &)> func_in,
		const unsigned char slot_num_in, const boost::uint64_t file_block_count);
	//send ctor
	request_file_block(const unsigned char slot_num,
		const boost::uint64_t block_num, const boost::uint64_t file_block_count);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
private:
	unsigned char slot_num; //used only for recv
	unsigned VLI_size;      //block number field size (bytes), used only for recv
};

class request_slot : public base
{
public:
	//recv ctor
	explicit request_slot(boost::function<bool (network::buffer &)> func_in);
	//send ctor
	explicit request_slot(const std::string & hash);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
};

class slot : public base
{
public:
	//recv ctor
	slot(boost::function<bool (network::buffer &)> func_in,
		const std::string & hash_in);
	//send ctor
	slot(const unsigned char slot_num, const boost::uint64_t file_size,
		const std::string & root_hash, const bit_field & tree_BF,
		const bit_field & file_BF);
	virtual bool expect(network::buffer & recv_buf);
	virtual status recv(network::buffer & recv_buf);
private:
	std::string hash; //used only for recv
	bool checked;     //true when file_size/root_hash checked, used only for recv
};
}//end namespace message
#endif
