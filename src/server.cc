#include "server.h"

server::server():
	blacklist_state(0),
	connections(0),
	stop_threads(false),
	threads(0)
{
	FD_ZERO(&master_FDS);
	max_connections = DB_Server_Preferences.get_max_connections();
	Speed_Calculator.set_speed_limit(DB_Server_Preferences.get_speed_limit_uint());

	#ifdef WIN32
	//start winsock
	WORD wsock_ver = MAKEWORD(1,1);
	WSADATA wsock_data;
	int startup;
	if((startup = WSAStartup(wsock_ver, &wsock_data)) != 0){
		logger::debug(LOGGER_P1,"winsock startup error ", startup);
		exit(1);
	}
	#endif

	boost::thread T(boost::bind(&server::main_thread, this));
}

server::~server()
{
	stop_threads = true;
	raise(SIGINT); //force select() to return
	while(threads){
		portable_sleep::yield();
	}

	server_buffer::destroy();

	#ifdef WIN32
	//cleanup winsock
	WSACleanup();
	#endif
}

void server::check_blacklist()
{
	if(DB_blacklist::modified(blacklist_state)){
		sockaddr_in temp_addr;
		socklen_t len = sizeof(temp_addr);
		for(int socket_FD = 0; socket_FD <= FD_max; ++socket_FD){
			if(FD_ISSET(socket_FD, &master_FDS)){
				getpeername(socket_FD, (sockaddr*)&temp_addr, &len);
				std::string IP(inet_ntoa(temp_addr.sin_addr));
				if(DB_blacklist::is_blacklisted(IP)){
					logger::debug(LOGGER_P1,"disconnecting blacklisted IP ",IP);
					disconnect(socket_FD);
				}
			}
		}
	}
}

void server::current_uploads(std::vector<upload_info> & info)
{
	server_buffer::current_uploads(info);
}

void server::disconnect(const int & socket_FD)
{
	logger::debug(LOGGER_P1,"disconnecting socket ",socket_FD);
	FD_CLR(socket_FD, &master_FDS);
	--connections;

	#ifdef WIN32
	closesocket(socket_FD);
	#else
	close(socket_FD);
	#endif

	server_buffer::erase(socket_FD);

	//reduce FD_max if possible
	for(int x = FD_max; x != 0; --x){
		if(FD_ISSET(x, &master_FDS)){
			FD_max = x;
			break;
		}
	}
}

int server::get_max_connections()
{
	return max_connections;
}

void server::set_max_connections(int max_connections_in)
{
	max_connections = max_connections_in;
	DB_Server_Preferences.set_max_connections(max_connections);
	while(connections > max_connections){
		for(int socket_FD = 0; socket_FD <= FD_max; ++socket_FD){
			if(FD_ISSET(socket_FD, &master_FDS)){
				disconnect(socket_FD);
				break;
			}
		}
	}
}

std::string server::get_share_directory()
{
	return DB_Server_Preferences.get_share_directory();
}

void server::set_share_directory(const std::string & share_directory)
{
	DB_Server_Preferences.set_share_directory(share_directory);
	Server_Index.set_share_directory(share_directory);
}

std::string server::get_speed_limit()
{
	std::ostringstream sl;
	if(Speed_Calculator.get_speed_limit() == std::numeric_limits<unsigned int>::max()){
		sl << "0";
	}else{
		sl << Speed_Calculator.get_speed_limit() / 1024;
	}
	return sl.str();
}

void server::set_speed_limit(const std::string & speed_limit)
{
	std::stringstream ss(speed_limit);
	unsigned int speed;
	ss >> speed;
	speed *= 1024;
	if(speed == 0){
		speed = speed - 1;
	}
	DB_Server_Preferences.set_speed_limit(speed);
	Speed_Calculator.set_speed_limit(speed);
}

bool server::is_indexing()
{
	return Server_Index.is_indexing();
}

void server::new_connection(const int & listener)
{
	sockaddr_in remoteaddr;
	socklen_t len = sizeof(remoteaddr);
	int new_FD = accept(listener, (sockaddr *)&remoteaddr, &len);
	if(new_FD == -1){
		#ifdef WIN32
		logger::debug(LOGGER_P1,"winsock error ",WSAGetLastError());
		#else
		perror("accept");
		#endif
		return;
	}

	std::string new_IP(inet_ntoa(remoteaddr.sin_addr));

	//do not allow clients to connect that are blacklisted
	if(DB_blacklist::is_blacklisted(new_IP)){
		logger::debug(LOGGER_P1,"stopping connection from blacklisted IP ",new_IP);
		return;
	}

	//make sure the client isn't already connected
	sockaddr_in temp_addr;
	for(int socket_FD = 0; socket_FD <= FD_max; ++socket_FD){
		if(FD_ISSET(socket_FD, &master_FDS)){
			getpeername(socket_FD, (sockaddr*)&temp_addr, &len);
			if(strcmp(new_IP.c_str(), inet_ntoa(temp_addr.sin_addr)) == 0){
				logger::debug(LOGGER_P1,"server ",new_IP," attempted multiple connections");
				#ifdef WIN32
				closesocket(new_FD);
				#else
				close(new_FD);
				#endif
				return;
			}
		}
	}

	if(connections < max_connections){
		++connections;
		server_buffer::new_connection(new_FD, new_IP);
		FD_SET(new_FD, &master_FDS);
		if(new_FD > FD_max){
			FD_max = new_FD;
		}
		logger::debug(LOGGER_P1,"client ",inet_ntoa(remoteaddr.sin_addr)," socket ",new_FD," connected");
	}else{
		#ifdef WIN32
		closesocket(new_FD);
		#else
		close(new_FD);
		#endif
	}

	return;
}

void server::main_thread()
{
	++threads;

	//set listening socket
	unsigned int listener;
	if((listener = socket(PF_INET, SOCK_STREAM, 0)) == -1){
		#ifdef WIN32
		logger::debug(LOGGER_P1,"winsock error ",WSAGetLastError());
		#else
		perror("socket");
		#endif
		exit(1);
	}

	/*
	Reuse port if it's currently in use. It can take up to a minute for linux to
	deallocate the port if this isn't used.
	*/
	bool yes = true;
	if(setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(int)) == -1){
		#ifdef WIN32
		logger::debug(LOGGER_P1,"winsock error ",WSAGetLastError());
		#else
		perror("setsockopt");
		#endif
		exit(1);
	}

	sockaddr_in myaddr;                        //server address
	myaddr.sin_family = AF_INET;               //set ipv4
	myaddr.sin_addr.s_addr = INADDR_ANY;       //set to listen on all available interfaces
	myaddr.sin_port = htons(global::P2P_PORT); //set listening port
	memset(&(myaddr.sin_zero), '\0', 8);

	//set listener info to what we set above
	if(bind(listener, (sockaddr *)&myaddr, sizeof(myaddr)) == -1){
		#ifdef WIN32
		logger::debug(LOGGER_P1,"winsock error ",WSAGetLastError());
		#else
		perror("bind");
		#endif
		exit(1);
	}

	//start listener socket listening
	if(listen(listener, 10) == -1){
		#ifdef WIN32
		logger::debug(LOGGER_P1,"winsock error ",WSAGetLastError());
		#else
		perror("listen");
		#endif
		exit(1);
	}
	logger::debug(LOGGER_P1,"created listening socket ",listener);

	FD_SET(listener, &master_FDS);
	FD_max = listener;

	char recv_buff[global::S_MAX_SIZE*global::PIPELINE_SIZE];
	int n_bytes;
	int send_limit = 0;
	while(true){
		if(stop_threads){
			break;
		}

		check_blacklist();

		/*
		This if(send_pending) exists to saves CPU time by not checking if sockets
		are ready to write when we don't need to write anything.
		*/
		if(server_buffer::get_send_pending() != 0){
			read_FDS = master_FDS;
			write_FDS = master_FDS;
			if(select(FD_max+1, &read_FDS, &write_FDS, NULL, NULL) == -1){
				//gprof will send PROF signal, this will ignore it
				if(errno != EINTR){
					#ifdef WIN32
					logger::debug(LOGGER_P1,"winsock error ",WSAGetLastError());
					#else
					perror("server select");
					#endif
					exit(1);
				}
			}
		}else{
			FD_ZERO(&write_FDS);
			read_FDS = master_FDS;
			if(select(FD_max+1, &read_FDS, NULL, NULL, NULL) == -1){
				//gprof will send PROF signal, this will ignore it
				if(errno != EINTR){
					#ifdef WIN32
					logger::debug(LOGGER_P1,"winsock error ",WSAGetLastError());
					#else
					perror("server select");
					#endif
					exit(1);
				}
			}
		}

		for(int socket_FD = 0; socket_FD <= FD_max; ++socket_FD){
			if(FD_ISSET(socket_FD, &read_FDS)){
				if(FD_ISSET(listener, &read_FDS)){
					//new client connected
					new_connection(listener);
				}else{
					//existing socket sending data
					if((n_bytes = recv(socket_FD, recv_buff, global::S_MAX_SIZE*global::PIPELINE_SIZE, 0)) <= 0){
						//error or disconnect
						disconnect(socket_FD);
						continue;
					}else{
						//incoming data from client socket
						server_buffer::process(socket_FD, recv_buff, n_bytes);
					}
				}
			}

			//do not check for writes on listener
			if(FD_ISSET(socket_FD, &write_FDS) && socket_FD != listener){
				std::string * buff = &server_buffer::get_send_buff(socket_FD);
				if(!buff->empty()){
					send_limit = Speed_Calculator.rate_control(buff->size());
					if((n_bytes = send(socket_FD, buff->c_str(), send_limit, MSG_NOSIGNAL)) < 0){
						disconnect(socket_FD);
					}else{
						//remove bytes sent from buffer
						Speed_Calculator.update(n_bytes);
						buff->erase(0, n_bytes);
						server_buffer::post_send(socket_FD);
					}
				}
			}
		}
	}

	--threads;
}

int server::total_speed()
{
	Speed_Calculator.update(0);
	return Speed_Calculator.speed();
}
