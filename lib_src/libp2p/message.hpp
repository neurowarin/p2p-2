#ifndef H_MESSAGE
#define H_MESSAGE

//custom
#include "encryption.hpp"
#include "protocol.hpp"

//include
#include <boost/shared_ptr.hpp>
#include <network/network.hpp>

//standard
#include <list>
#include <map>

namespace message{

//abstract base class for all messages
class base
{
public:
	//function to handle the message when it's received
	boost::function<bool (boost::shared_ptr<base>)> func;

	//bytes that have been sent or received
	network::buffer buf;

	/*
	encrypt:
		Returns true if message should be encrypted before sending. The default is
		true. The key_exchange messages override this and return false.
	expects:
		Returns true if recv() expects message on front of CI.recv_buf.
	recv:
		Returns true if incoming message received. False if incomplete message or
		host blacklisted.
	encrypt
	*/
	virtual bool encrypt();
	virtual bool expects(network::connection_info & CI) = 0;
	virtual bool recv(network::connection_info & CI) = 0;
};

//the composite message is used to handle multiple possible responses
class composite : public base
{
public:
	//add messages to expect() and recv()
	void add(boost::shared_ptr<base> M);
	virtual bool expects(network::connection_info & CI);
	virtual bool recv(network::connection_info & CI);
private:
	boost::shared_ptr<base> response;
	std::vector<boost::shared_ptr<base> > possible_response;
};

class error : public base
{
public:
	//ctor to recv error
	explicit error(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//ctor to send error
	error();
	virtual bool expects(network::connection_info & CI);
	virtual bool recv(network::connection_info & CI);
};

class initial : public base
{
public:
	//ctor to recv message
	explicit initial(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//ctor to send message
	explicit initial(const std::string peer_ID);
	virtual bool expects(network::connection_info & CI);
	virtual bool recv(network::connection_info & CI);
};

class key_exchange_p_rA : public base
{
public:
	//ctor to recv message
	explicit key_exchange_p_rA(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//ctor to send message
	explicit key_exchange_p_rA(encryption & Encryption);
	virtual bool encrypt();
	virtual bool expects(network::connection_info & CI);
	virtual bool recv(network::connection_info & CI);
};

class key_exchange_rB : public base
{
public:
	//ctor to recv message
	explicit key_exchange_rB(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//ctor to send message
	explicit key_exchange_rB(encryption & Encryption);
	virtual bool encrypt();
	virtual bool expects(network::connection_info & CI);
	virtual bool recv(network::connection_info & CI);
};

class request_slot : public base
{
public:
	//ctor to recv message
	explicit request_slot(boost::function<bool (boost::shared_ptr<base>)> func_in);
	//ctor to send message
	explicit request_slot(const std::string & hash);
	virtual bool expects(network::connection_info & CI);
	virtual bool recv(network::connection_info & CI);
};

class slot : public base
{
public:
	//ctor to recv message
	slot(boost::function<bool (boost::shared_ptr<base>)> func_in,
		const std::string & hash_in);
	//ctor to send message
	slot(const unsigned char slot_num, unsigned char status,
		const boost::uint64_t file_size, const std::string & root_hash);
	virtual bool expects(network::connection_info & CI);
	virtual bool recv(network::connection_info & CI);
private:
	//hash requested
	std::string hash;
	/*
	Set to true when file_size and root_hash checked. This is used to save
	processing from computing the hash multiple times.
	*/
	bool checked;
};
}//end namespace message
#endif