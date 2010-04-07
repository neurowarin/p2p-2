#include <network/network.hpp>

network::nstream::nstream():
	socket_FD(-1)
{

}

network::nstream::nstream(const endpoint & E):
	socket_FD(-1)
{
	open(E);
}

network::nstream::nstream(const int socket_FD_in):
	socket_FD(socket_FD_in)
{

}

network::nstream::~nstream()
{
	close();
}

void network::nstream::close()
{
	if(socket_FD != -1){
		if(::close(socket_FD) == -1){
			LOGGER << errno;
		}
		socket_FD = -1;
	}
}

bool network::nstream::is_open() const
{
	return socket_FD != -1;
}

bool network::nstream::is_open_async()
{
	int opt_val = 1;
	socklen_t opt_len = sizeof(opt_val);
	if(getsockopt(socket_FD, SOL_SOCKET, SO_ERROR,
		reinterpret_cast<char *>(&opt_val), &opt_len) == 0)
	{
		//we got option
		if(opt_val == 0){
			return true;
		}else{
			LOGGER << errno;
			return false;
		}
	}else{
		//error getting option
		LOGGER << errno;
		return false;
	}
}

std::string network::nstream::local_IP()
{
	if(socket_FD == -1){
		return "";
	}
	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if(getsockname(socket_FD, reinterpret_cast<sockaddr *>(&addr), &addrlen) == -1){
		LOGGER << errno;
		close();
		return "";
	}
	char buf[INET6_ADDRSTRLEN];
	if(getnameinfo(reinterpret_cast<sockaddr *>(&addr), addrlen, buf,
		sizeof(buf), NULL, 0, NI_NUMERICHOST) == -1)
	{
		LOGGER << errno;
		close();
		return "";
	}
	return buf;
}

std::string network::nstream::local_port()
{
	if(socket_FD == -1){
		return "";
	}
	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if(getsockname(socket_FD, reinterpret_cast<sockaddr *>(&addr), &addrlen) == -1){
		LOGGER << errno;
		close();
		return "";
	}
	char buf[6];
	if(getnameinfo(reinterpret_cast<sockaddr *>(&addr), addrlen, NULL, 0, buf,
		sizeof(buf), NI_NUMERICSERV) == -1)
	{
		LOGGER << errno;
		close();
		return "";
	}
	return buf;
}

void network::nstream::open(const endpoint & E)
{
	assert(E.type() == tcp);
	close();
	if((socket_FD = ::socket(E.ai.ai_family, E.ai.ai_socktype, E.ai.ai_protocol)) == -1){
		LOGGER << errno;
		close();
	}
	if(::connect(socket_FD, E.ai.ai_addr, E.ai.ai_addrlen) != 0){
		LOGGER << errno;
		close();
	}
}

void network::nstream::open_async(const endpoint & E)
{
	close();
	if((socket_FD = ::socket(E.ai.ai_family, E.ai.ai_socktype,
		E.ai.ai_protocol)) == -1)
	{
		LOGGER << errno;
		exit(1);
	}
	set_non_blocking();
	/*
	FreeBSD:
		If connect returns 0 the socket MAY or MAY NOT be connected.
	Windows/Linux:
		If connect returns 0 the socket connected immediately.

	Because of the inconsistency above we check for writeability because it
	works on FreeBSD, Windows, and Linux.
	*/
	if(::connect(socket_FD, E.ai.ai_addr, E.ai.ai_addrlen) != 0){
		//socket in progress of connecting
		if(errno != EINPROGRESS && errno != EWOULDBLOCK){
			LOGGER << errno;
			exit(1);
		}
	}
}

int network::nstream::recv(buffer & buf, const int max_transfer)
{
	assert(max_transfer > 0);
	if(max_transfer > MTU){
		buf.tail_reserve(MTU);
	}else{
		buf.tail_reserve(max_transfer);
	}
	if(socket_FD == -1){
		//socket previously disconnected, errno might not be valid here
		return 0;
	}else{
		int n_bytes = ::recv(socket_FD, reinterpret_cast<char *>(buf.tail_start()),
			buf.tail_size(), MSG_NOSIGNAL);
		if(n_bytes == -1){
			LOGGER << errno;
		}else if(n_bytes == 0){
			close();
			buf.tail_reserve(0);
		}else{
			buf.tail_resize(n_bytes);
		}
		return n_bytes;
	}
}

std::string network::nstream::remote_IP()
{
	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if(getpeername(socket_FD, reinterpret_cast<sockaddr *>(&addr), &addrlen) == -1){
		LOGGER << errno;
		close();
		return "";
	}
	char buf[INET6_ADDRSTRLEN];
	if(getnameinfo(reinterpret_cast<sockaddr *>(&addr), addrlen, buf,
		sizeof(buf), NULL, 0, NI_NUMERICHOST) == -1)
	{
		LOGGER << errno;
		close();
		return "";
	}
	return buf;
}

std::string network::nstream::remote_port()
{
	sockaddr_storage addr;
	socklen_t addrlen = sizeof(addr);
	if(getpeername(socket_FD, reinterpret_cast<sockaddr *>(&addr), &addrlen) == -1){
		LOGGER << errno;
		close();
		return "";
	}
	char buf[6];
	if(getnameinfo(reinterpret_cast<sockaddr *>(&addr), addrlen, NULL, 0, buf,
		sizeof(buf), NI_NUMERICSERV) == -1)
	{
		LOGGER << errno;
		close();
		return "";
	}
	return buf;
}

int network::nstream::send(buffer & buf, int max_transfer)
{
	assert(max_transfer > 0);
	if(max_transfer > buf.size()){
		max_transfer = buf.size();
	}
	if(max_transfer > MTU){
		max_transfer = MTU;
	}

	if(socket_FD == -1){
		//socket previously disconnected, errno might not be valid here
		return 0;
	}else{
		int n_bytes = ::send(socket_FD, reinterpret_cast<char *>(buf.data()),
			max_transfer, MSG_NOSIGNAL);
		if(n_bytes == -1){
			LOGGER << errno;
		}else if(n_bytes == 0){
			close();
		}else{
			buf.erase(0, n_bytes);
		}
		return n_bytes;
	}
}

void network::nstream::set_non_blocking()
{
	if(socket_FD != -1){
		#ifdef _WIN32
		u_long mode = 1;
		if(ioctlsocket(socket_FD, FIONBIO, &mode) == -1){
			LOGGER << errno;
			close();
		}
		#else
		if(fcntl(socket_FD, F_SETFL, O_NONBLOCK) == -1){
			LOGGER << errno;
			close();
		}
		#endif
	}
}

int network::nstream::socket()
{
	return socket_FD;
}
