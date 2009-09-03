//include
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>
#include <convert.hpp>
#include <network/network.hpp>

class connection
{
public:
	connection(network::sock & S)
	{
		if(S.direction == network::OUTGOING){
			S.send_buff.append('x');
		}
	}

	void recv_call_back(network::sock & S)
	{
		if(S.direction == network::OUTGOING){
			if(S.recv_buff.size() != 1){
				LOGGER; exit(1);
			}
			if(S.recv_buff[0] != 'y'){
				LOGGER; exit(1);
			}
			S.disconnect_flag = true;
		}else{
			if(S.recv_buff.size() != 1){
				LOGGER; exit(1);
			}
			if(S.recv_buff[0] != 'x'){
				LOGGER; exit(1);
			}
			S.send_buff.append('y');
			S.recv_buff.clear();
		}
	}

	void send_call_back(network::sock & S)
	{
		//nothing to do after a send
	}
};

boost::mutex Connection_mutex;
std::map<int, boost::shared_ptr<connection> > Connection;

void connect_call_back(network::sock & S)
{
	boost::mutex::scoped_lock lock(Connection_mutex);
	std::string direction;
	if(S.direction == network::INCOMING){
		direction = "in ";
	}else{
		direction = "out";
	}
	LOGGER << direction << " H<" << S.host << "> IP<" << S.IP << "> P<"
		<< S.port << "> S<" << S.socket_FD << ">";
	std::pair<std::map<int, boost::shared_ptr<connection> >::iterator, bool>
		ret = Connection.insert(std::make_pair(S.socket_FD, new connection(S)));
	S.recv_call_back = boost::bind(&connection::recv_call_back, ret.first->second.get(), _1);
	S.send_call_back = boost::bind(&connection::send_call_back, ret.first->second.get(), _1);
}

void disconnect_call_back(network::sock & S)
{
	boost::mutex::scoped_lock lock(Connection_mutex);
	std::string direction;
	if(S.direction == network::INCOMING){
		direction = "in";
	}else{
		direction = "out";
	}
	LOGGER << direction << " H<" << S.host << "> IP<" << S.IP << "> P<"
		<< S.port << "> S<" << S.socket_FD << ">";
	if(Connection.erase(S.socket_FD) != 1){
		LOGGER << "disconnect called for socket that never connected";
		exit(1);
	}
}

void failed_connect_call_back(network::sock & S)
{
	assert(S.sock_error != network::NO_ERROR);
	std::string reason;
	if(S.sock_error == network::FAILED_RESOLVE){
		reason = "failed resolve";
	}else if(S.sock_error == network::MAX_CONNECTIONS){
		reason = "max connections";
	}else if(S.sock_error == network::TIMEOUT){
		reason = "timeout";
	}else if(S.sock_error == network::OTHER){
		reason = "other";
	}else{
		LOGGER << "unrecognized failure reason";
		exit(1);
	}
	LOGGER << "H<" << S.host << "> P<" << S.port << ">" << " R<" << reason << ">";
	exit(1);
}

int main()
{
	network::proactor Proactor(
		&connect_call_back,
		&disconnect_call_back,
		&failed_connect_call_back,
		"65001"
	);

	for(int x=0; x<32; ++x){
		Proactor.connect("127.0.0.1", "65001");
	}

	bool hack = true; //connections won't immediately be non-zero
	while(Proactor.connections() != 0 || hack){
		LOGGER << "C<" << Proactor.connections() << "> "
			<< "IC<" << Proactor.incoming_connections() << "> "
			<< "OC<" << Proactor.outgoing_connections() << "> "
			<< "D<" << convert::size_SI(Proactor.download_rate()) << "> "
			<< "U<" << convert::size_SI(Proactor.upload_rate()) << ">";
		boost::this_thread::sleep(boost::posix_time::milliseconds(1000));
		hack = false;
	}
}
