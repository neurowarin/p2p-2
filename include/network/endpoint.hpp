#ifndef H_NETWORK_ENDPOINT
#define H_NETWORK_ENDPOINT

//custom
#include "protocol.hpp"
#include "start.hpp"
#include "system_include.hpp"

//include
#include <boost/optional.hpp>
#include <convert.hpp>
#include <logger.hpp>

//standard
#include <set>

namespace network{

//classes which need access to private addrinfo
class listener;
class ndgram;
class nstream;

class endpoint
{
	network::start Start;

	friend class listener;
	friend class ndgram;
	friend class nstream;
	friend std::set<endpoint> get_endpoint(const std::string & host,
		const std::string & port);
	friend boost::optional<network::endpoint> bin_to_endpoint(
		const std::string & addr, const std::string & port);
public:
	//copy ctor
	endpoint(const endpoint & E);

	/*
	IP:
		Returns IP address.
	IP_bin:
		Returns binary IP address in big-endian. This will be 4 bytes for IPv4 and
		16 bytes for IPv6.
	port:
		Returns port.
	port_bin:
		Returns binary port in big-endian. This will always be 2 bytes.
	type:
		Returns type. (tcp or udp)

	*/
	std::string IP() const;
	std::string IP_bin() const;
	std::string port() const;
	std::string port_bin() const;
	socket_t type() const;
	version_t version() const;

	//operators
	endpoint & operator = (const endpoint & rval);
	bool operator < (const endpoint & rval) const;
	bool operator == (const endpoint & rval) const;
	bool operator != (const endpoint & rval) const;

private:
	explicit endpoint(const addrinfo * ai_in);

	//wrapped info needed to connect to a host
	addrinfo ai;
	sockaddr_storage sas;

	void copy(const addrinfo * ai_in);
};

/*
Returns set of possible endpoints for the specified host and port. The
std::set is used because the getaddrinfo function can return duplicates.
*/
extern std::set<endpoint> get_endpoint(const std::string & host,
	const std::string & port);

/*
Converts bytes to endpoint. A IPv6 endpoing won't be returned if the system
doesn't support IPv6.
Note: addr must be 4 or 16 bytes. port must be 2 bytes.
*/
extern boost::optional<network::endpoint> bin_to_endpoint(
	const std::string & addr, const std::string & port);
}//end of namespace network
#endif
