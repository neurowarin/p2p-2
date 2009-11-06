//include
#include <network/network.hpp>

int main()
{
	//disable test
	return 0;

	std::set<network::endpoint> E = network::get_endpoint("google.com", "80", network::tcp);
	network::nstream N;
	for(std::set<network::endpoint>::iterator cur = E.begin(), end = E.end();
		cur != end; ++cur)
	{
		N.open(*cur);
		if(N.is_open()){
			break;
		}
	}
	if(!N.is_open()){
		LOGGER << N.error(); exit(1);
	}
	network::buffer B(
		"GET / HTTP/1.1\r\n"
		"Connection: close\r\n\r\n"
	);
	while(!B.empty() && N.send(B) > 0);
	if(!N.is_open()){
		LOGGER << N.error(); exit(1);
	}
	while(N.recv(B) > 0);
	if(!N.is_open()){
		LOGGER << N.error(); exit(1);
	}
	std::cout << "recv buff size: " << B.size() << "\n";
}