//boost
#include <boost/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

//network
#include <network.hpp>

/*
Must be included after network because network 
*/
#include <portable_sleep.hpp>

const std::string listen_port = "6969";

class connection
{
public:
	connection(network::socket & Socket)
	{
		if(Socket.port != listen_port){
			//client starts by sending a request
			Socket.send_buff.append('x');
		}
	}

	void recv_call_back(network::socket & Socket)
	{
		if(Socket.port != listen_port){
			//client
			if(Socket.recv_buff.size() != 1){
				LOGGER; exit(1);
			}
			if(Socket.recv_buff[0] != 'y'){
				LOGGER; exit(1);
			}
			Socket.disconnect_flag = true;
		}else{
			//server
			if(Socket.recv_buff.size() != 1){
				LOGGER; exit(1);
			}
			if(Socket.recv_buff[0] != 'x'){
				LOGGER; exit(1);
			}
			Socket.send_buff.append('y');
			Socket.recv_buff.clear();
		}
	}

	void send_call_back(network::socket & Socket)
	{
		//nothing to do after a send
	}
};

boost::mutex Connection_mutex;
std::map<int, boost::shared_ptr<connection> > Connection;

void connect_call_back(network::socket & Socket)
{
	//simulate sessions that take 1 second to complete
	portable_sleep::ms(1000);

	boost::mutex::scoped_lock lock(Connection_mutex);
	if(Socket.port != listen_port){
		LOGGER << "h<" << Socket.host << "> IP<" << Socket.IP << "> p<"
			<< Socket.port << "> s<" << Socket.socket_FD << ">";
	}else{
		LOGGER << "h<" << Socket.host << "> IP<" << Socket.IP << "> p"
			<< Socket.port << "> s<" << Socket.socket_FD << ">";
	}
	Connection.insert(std::make_pair(Socket.socket_FD, new connection(Socket)));
	Socket.recv_call_back = boost::bind(&connection::recv_call_back, Connection[Socket.socket_FD].get(), _1);
	Socket.send_call_back = boost::bind(&connection::send_call_back, Connection[Socket.socket_FD].get(), _1);
}

void disconnect_call_back(network::socket & Socket)
{
	//simulate sessions that take 1 second to complete
	portable_sleep::ms(1000);

	boost::mutex::scoped_lock lock(Connection_mutex);
	LOGGER << "h<" << Socket.host << "> IP<" << Socket.IP << "> p<"
		<< Socket.port << "> s<" << Socket.socket_FD << ">";
	Connection.erase(Socket.socket_FD);
}

void failed_connect_call_back(const std::string & host, const std::string & port)
{
	//simulate sessions that take 1 second to complete
	portable_sleep::ms(1000);

	LOGGER << "h<" << host << "> p<" << port << ">";
}

int main()
{
	network::io_service Network(
		&failed_connect_call_back,
		&connect_call_back,
		&disconnect_call_back,
		"6969"
	);

	int cnt = 0;
	while(cnt < 1){
		Network.connect("::1", "6969");
		Network.connect("127.0.0.1", "6969");
		Network.connect(":1:2:3:4", "1234");
		Network.connect("192.168.1.113", "1234");
		++cnt;
	}

	//while(Network.connections() != 0){
		LOGGER << "connections: " << Network.connections()
		<< " incoming: " << Network.incoming_connections()
		<< " outgoing: " << Network.outgoing_connections();
		portable_sleep::ms(1000);
	//}

	portable_sleep::ms(10*1000);
}

