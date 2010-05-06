//include
#include <logger.hpp>
#include <net/net.hpp>

int fail(0);

int main()
{
	{//BEGIN IPv4
	std::set<net::endpoint> E = net::get_endpoint("127.0.0.1", "0");
	if(E.empty()){
		LOG; ++fail;
		return fail;
	}

	//copy endpoint with binary functions
	net::endpoint test_1 = *E.begin();
	std::string IP_bin = test_1.IP_bin();
	std::string port_bin = test_1.port_bin();
	boost::optional<net::endpoint> test_2 = net::bin_to_endpoint(IP_bin, port_bin);
	if(!test_2){
		LOG; ++fail;
		return fail;
	}

	//check if endpoints are the same
	if(test_1 != *test_2){
		LOG; ++fail;
	}

	if(test_1.version() != net::IPv4){
		LOG; ++fail;
	}
	}//END IPv4

	{//BEGIN IPv6
	std::set<net::endpoint> E = net::get_endpoint("::1", "0");

	//if fail to get endpoint we probably don't have IPv6 support
	if(!E.empty()){
		//copy endpoint with binary functions
		net::endpoint test_1 = *E.begin();
		std::string IP_bin = test_1.IP_bin();
		std::string port_bin = test_1.port_bin();
		boost::optional<net::endpoint> test_2 = net::bin_to_endpoint(IP_bin, port_bin);
		if(!test_2){
			LOG; ++fail;
			return fail;
		}

		//check if endpoints are the same
		if(test_1 != *test_2){
			LOG; ++fail;
		}

		if(test_1.version() != net::IPv6){
			LOG; ++fail;
		}
	}
	}//END IPv6

	return fail;
}
