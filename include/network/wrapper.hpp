#ifndef H_NETWORK_WRAPPER
#define H_NETWORK_WRAPPER

//include
#include <logger.hpp>

//networking
#ifdef _WIN32
	#define MSG_NOSIGNAL 0  //disable SIGPIPE on send() to disconnected socket
	#define FD_SETSIZE 1024 //max number of sockets in fd_set
	#include <ws2tcpip.h>

	//close is closesocket in winsock
	#define close closesocket

	/*
	Macros for BSD errno compatability.
	http://msdn.microsoft.com/en-us/library/ms740668(VS.85).aspx
	Note: Use the BSD style macro then redefine it here.
	*/
	#define errno WSAGetLastError()
	#define EMFILE WSAEMFILE
	#define EINTR WSAEINTR
#else
	#include <arpa/inet.h>
	#include <fcntl.h>
	#include <netdb.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <unistd.h>
#endif

namespace network{
class wrapper
{
public:
	/*
		This object wraps getaddrinfo for the purpose of automatically free'ing
	memory allocated by the getaddrinfo function.
	*/
	class info : private boost::noncopyable
	{
	public:
		/*
		The host and/or port can be NULL. The host is generally left NULL when
		setting up addrinfo for a listener that listens on all interfaces.

		The ai_family parameter is the address family to use for the socket.
		It can be AF_INET (IPv4), AF_INET6 (IPv6), or AF_UNSPEC (IPv4 or IPv6).
		*/
		info(const char * host, const char * port, const int ai_family = AF_UNSPEC)
		{
			if(host != NULL || port != NULL){
				addrinfo hints;
				std::memset(&hints, 0, sizeof(hints));
				hints.ai_family = ai_family;
				hints.ai_socktype = SOCK_STREAM; //TCP
				if(host == NULL){
					//fill in IP (all available interfaces)
					hints.ai_flags = AI_PASSIVE;
				}
				if(getaddrinfo(host, port, &hints, &res_begin) != 0){
					res_begin = NULL;
					res_current = NULL;
				}else{
					res_current = res_begin;
				}
			}else{
				res_begin = NULL;
				res_current = NULL;
			}
		}

		~info()
		{
			if(res_begin != NULL){
				freeaddrinfo(res_begin);
			}
		}

		/*
		Returns addrinfo for the current node in the list.
		*/
		addrinfo * get_addrinfo() const
		{
			assert(res_current != NULL);
			return res_current;
		}

		/*
		Traverses to the next addrinfo in the list. After calling this the
		resolved() function should be called to see if at the end of the list.
		*/
		void next_addrinfo()
		{
			assert(res_current != NULL);
			res_current = res_current->ai_next;
		}

		/*
		Returns true if not at the end of the list.
		Note: If addrinfo struct can't be filled then this will return false
			right after instantiation.
		*/
		bool resolved() const
		{
			return res_current != NULL;
		}

	private:
		/*
		The addrinfo struct is a linked list. The pointer to the next link is
		the member pointer ai_next. The list will be more than one element long
		if the host is multi-homed (domain resolves to both IPv4 and IPv6
		address).

		res_begin: Pointer to beginning of address list.
		res_current: Pointer to current node of address list.
		*/
		addrinfo * res_begin;
		addrinfo * res_current;
	};

	/* Accept connections on a listener.
	listener_FD blocking:
		Blocks until there is an incoming connection on the specified listener_FD.

	listener_FD non-blocking:
		Returns the socket of an incoming connection or -1 if there are no
		incoming connections to accept.
	*/
	static int accept_socket(const int listener_FD)
	{
		sockaddr_storage remoteaddr;
		socklen_t len = sizeof(remoteaddr);
		return accept(listener_FD, (sockaddr *)&remoteaddr, &len);
	}

	/* Check if asynchronous connection attempt succeeded.
	When a non-blocking socket in progress of connecting becomes writeable this
	function should be called to see if the connection attempt succeeded. Returns
	true if succeeded, else false.
	*/
	static bool async_connect_succeeded(const int socket_FD)
	{
		#ifdef _WIN32
		char optval = 1;
		#else
		int optval = 1;
		#endif
		int optlen = sizeof(optval);
		getsockopt(socket_FD, SOL_SOCKET, SO_ERROR, &optval, (socklen_t *)&optlen);
		return optval == 0;
	}

	/*
	Binds a socket to the specified addrinfo. Return false if bind fails.
	Note: This is generally only called when setting up a listener. Usually we
		let the OS take care of this when making an outbound connection.
	*/
	static bool bind_socket(const int socket_FD, const info & Info)
	{
		return bind(socket_FD, Info.get_addrinfo()->ai_addr,
			Info.get_addrinfo()->ai_addrlen) != -1;
	}

	/* Make connection attempt.
	non-blocking socket_FD (asynchronous connection):
		Returns true if socket connected right away (very rare but can happen).
		Returns false if socket in progress of connecting. Socket will become
		writeable when it connect or fails to connect. Check to see if async
		connected succeeded with async_connect_succeeded().

	blocking socket_FD (synchronous connection):
		Returns true if socket connected, false if failed to connect.
		Note: This blocks until connection made or connection attempt
			timed out.
	*/
	static bool connect_socket(const int socket_FD, const info & Info)
	{
		return connect(socket_FD, Info.get_addrinfo()->ai_addr,
			Info.get_addrinfo()->ai_addrlen) != -1;
	}

	/*
	Have the OS allocate a socket for the specified addrinfo.
	Return a socket or -1 on error.
	*/
	static int create_socket(const info & Info)
	{
		return socket(Info.get_addrinfo()->ai_family, Info.get_addrinfo()->ai_socktype,
			Info.get_addrinfo()->ai_protocol);
	}

	/*
	Returns IP address of remote host. Returns empty string IP cannot be looked up
	(can happen if socket is disconnected).
	*/
	static std::string get_IP(const int socket_FD)
	{
		/*
		Because IPv6 is used all socket addresses need to be gotten as IPv6. If
		the IP is IPv4 then it will be represented as ::ffff:<IPv4 address>
		*/
		sockaddr_storage addr;
		socklen_t len = sizeof(addr);
		getpeername(socket_FD, (sockaddr *)&addr, &len);
		char buff[INET6_ADDRSTRLEN];
		if(addr.ss_family == AF_INET){
			//IPv4 socket
			getnameinfo((sockaddr *)&addr, sizeof(sockaddr_in), buff, sizeof(buff),
				NULL, 0, NI_NUMERICHOST);
		}else if(addr.ss_family == AF_INET6){
			//IPv6 socket
			getnameinfo((sockaddr *)&addr, sizeof(sockaddr_in6), buff, sizeof(buff),
				NULL, 0, NI_NUMERICHOST);
		}

		/*
		A dualstack implementation (IPv4/IPv6 are the same code base) will return
		a IPv4 mapped address that will look like this ::ffff:123.123.123.123. We
		remove the front of it.
		*/
		std::string tmp(buff);
		if(tmp.find("::ffff:", 0) == 0){
			return std::string(tmp.begin()+7, tmp.end());
		}else{
			return buff;
		}
	}

	/*
	Same as above except this one takes info. This is good for getting the IP
	when the socket has not yet connected.
	*/
	static std::string get_IP(const info & Info)
	{
		char buff[INET6_ADDRSTRLEN];
		if(Info.get_addrinfo()->ai_family == AF_INET){
			//IPv4 socket
			getnameinfo((sockaddr *)Info.get_addrinfo()->ai_addr, sizeof(sockaddr_in),
				buff, sizeof(buff), NULL, 0, NI_NUMERICHOST);
		}else if(Info.get_addrinfo()->ai_family == AF_INET6){
			//IPv6 socket
			getnameinfo((sockaddr *)Info.get_addrinfo()->ai_addr, sizeof(sockaddr_in6),
				buff, sizeof(buff), NULL, 0, NI_NUMERICHOST);
		}

		/*
		A dualstack implementation (IPv4/IPv6 are the same code base) will return
		a IPv4 mapped address that will look like this ::ffff:123.123.123.123. We
		remove the front of it.
		*/
		std::string tmp(buff);
		if(tmp.find("::ffff:", 0) == 0){
			return std::string(tmp.begin()+7, tmp.end());
		}else{
			return buff;
		}
	}

	/*
	Returns port of remote host connected to. Returns empty string if port cannot
	be looked up (can happen if socket is disconnected).
	*/
	static std::string get_port(const int socket_FD)
	{
		sockaddr_in6 sa;
		socklen_t len = sizeof(sa);
		if(getpeername(socket_FD, (sockaddr *)&sa, &len) == -1){
			return "";
		}
		int port;
		port = ntohs(sa.sin6_port);
		std::stringstream ss;
		ss << port;
		return ss.str();
	}

	/*
	This takes care of "address already in use" errors that can happen when
	trying to bind to a port. The error can happen when a listener is setup,
	and the program crashes. The next time the program starts the OS hasn't
	deallocated the port so it won't let you bind to it.
	*/
	static void reuse_port(const int socket_FD)
	{
		#ifdef _WIN32
		const char optval = 1;
		#else
		int optval = 1;
		#endif
		int optlen = sizeof(optval);
		if(setsockopt(socket_FD, SOL_SOCKET, SO_REUSEADDR, &optval, optlen) == -1){
			LOGGER << errno;
			exit(1);
		}
	}

	//sets a socket to non-blocking
	static void set_non_blocking(const int socket_FD)
	{
		//set socket to non-blocking for async connect
		#ifdef _WIN32
		u_long mode = 1;
		ioctlsocket(socket_FD, FIONBIO, &mode);
		#else
		fcntl(socket_FD, F_SETFL, O_NONBLOCK);
		#endif
	}

	/*
	Not all operating systems have pipes, or a socket_pair function so this
	function is required for portability.
	*/
	static void socket_pair(int & selfpipe_read, int & selfpipe_write)
	{
		//find an available socket by trying to bind to different ports
		int tmp_listener;
		std::string port;
		for(int x=9090; x<65536; ++x){
			std::stringstream ss;
			ss << x;
			info Info("127.0.0.1", ss.str().c_str(), AF_INET);
			assert(Info.resolved());
			tmp_listener = create_socket(Info);
			if(tmp_listener == -1){
				LOGGER << "error creating socket pair";
				exit(1);
			}
			reuse_port(tmp_listener);
			if(bind_socket(tmp_listener, Info)){
				port = ss.str();
				break;
			}else{
				close(tmp_listener);
			}
		}

		if(listen(tmp_listener, 1) == -1){
			LOGGER << errno;
			exit(1);
		}

		//connect socket for writer end of the pair
		info Info("127.0.0.1", port.c_str(), AF_INET);
		assert(Info.resolved());
		selfpipe_write = create_socket(Info);
		if(selfpipe_write == -1){
			LOGGER << errno;
			exit(1);
		}
		if(!connect_socket(selfpipe_write, Info)){
			LOGGER << errno;
			exit(1);
		}

		//accept socket for reader end of the pair
		if((selfpipe_read = accept_socket(tmp_listener)) == -1){
			LOGGER << errno;
			exit(1);
		}
		close(tmp_listener);
	}

	/*
	Accept incoming connections via IPv4 on specified port. Returns -1 if there
	is an error.
	*/
	static int start_listener_IPv4(const std::string & port)
	{
		return start_listener(port, AF_INET);
	}

	/*
	Accept incoming connections via IPv6 on specified port. Returns -1 if there
	is an error.
	*/
	static int start_listener_IPv6(const std::string & port)
	{
		return start_listener(port, AF_INET6);
	}

	/*
	Accept incoming connections via IPv4 or IPv6 on specified port. Returns -1 if there
	is an error.
	*/
	static int start_listener_dual_stack(const std::string & port)
	{
		return start_listener(port, AF_UNSPEC);
	}

	/*
	Starts the listeners depending on what system this is. If a listener can't be
	started the socket_FD will be set to -1. If neither listener can be started
	then the program will be terminated. If a dualstack socket is started then
	both listeners will be equal.
	*/
	static void start_listeners(int & listener_IPv4, int & listener_IPv6, const std::string & port)
	{
		#ifdef _WIN32
		listener_IPv4 = wrapper::start_listener_IPv4(port);
		listener_IPv6 = wrapper::start_listener_IPv6(port);
		#else
		listener_IPv4 = wrapper::start_listener_dual_stack(port);
		listener_IPv6 = listener_IPv4;
		#endif
		if(listener_IPv4 == -1 && listener_IPv6 == -1){
			LOGGER << "failed to start either IPv4 or IPv6 listener";
			exit(1);
		}
	}

	//must be called before using any network functions
	static void start_networking()
	{
		#ifdef _WIN32
		WORD wsock_ver = MAKEWORD(2,2);
		WSADATA wsock_data;
		int startup;
		if((startup = WSAStartup(wsock_ver, &wsock_data)) != 0){
			LOGGER << "winsock startup error " << startup;
			exit(1);
		}
		#endif
	}

	//for every call to start_networking this function must be called
	static void stop_networking()
	{
		#ifdef _WIN32
		WSACleanup();
		#endif
	}
private:
	wrapper();

	static int start_listener(const std::string & port, const int ai_family)
	{
		wrapper::info Info(NULL, port.c_str(), ai_family);
		assert(Info.resolved());
		int listener = create_socket(Info);
		if(listener == -1){
			return -1;
		}
		reuse_port(listener);
		if(!bind_socket(listener, Info)){
			LOGGER << errno;
			exit(1);
		}
		int backlog = 32;
		if(listen(listener, backlog) == -1){
			LOGGER << errno;
			exit(1);
		}
		return listener;
	}
};
}//end of network namespace
#endif
