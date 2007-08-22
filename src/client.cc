//std
#include <algorithm>
#include <bitset>
#include <fstream>
#include <functional>
#include <iostream>
#include <iomanip>
#include <list>
#include <sstream>

#include "client.h"

client::client()
{
	fdmax = 0;
	ClientIndex.initialFillBuffer(downloadBuffer, serverHolder);

	if(!downloadBuffer.empty()){
		resumeDownloads = true;
	}

	sendPending = false;
	FD_ZERO(&masterfds);
}

inline void client::disconnect(const int & socketfd)
{
#ifdef DEBUG
	std::cout << "info: client disconnecting socket number " << socketfd << "\n";
#endif

	if(FD_ISSET(socketfd, &masterfds)){
		close(socketfd);
		FD_CLR(socketfd, &masterfds);
	}

	//reduce fdmax if possible
	for(int x=fdmax; x != 0; x--){
		if(FD_ISSET(x, &masterfds)){
			fdmax = x;
			break;
		}
	}

	{//begin lock scope
	boost::mutex::scoped_lock lock(sendBufferMutex);

	//erase any remaining buffer
	sendBuffer[socketfd].clear();

	//reduce send/receive buffers if possible
	while(sendBuffer.size() >= fdmax){
		sendBuffer.pop_back();
	}

	}//end lock scope
}

bool client::getDownloadInfo(std::vector<infoBuffer> & downloadInfo)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	if(downloadBuffer.size() == 0){
		return false;
	}

	//process sendQueue
	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){
		//calculate the percent complete
		float bytesDownloaded = iter0->getBlockCount() * global::BUFFER_SIZE;
		float fileSize = iter0->getFileSize();
		float percent = (bytesDownloaded / fileSize) * 100;
		int percentComplete = int(percent);

		//convert fileSize from Bytes to megaBytes
		fileSize = iter0->getFileSize() / 1024 / 1024;
		std::ostringstream fileSize_s;

		if(fileSize < 1){
			fileSize_s << std::setprecision(2) << fileSize << " mB";
		}
		else{
			fileSize_s << (int)fileSize << " mB";
		}

		//convert the download speed to kiloBytes
		int downloadSpeed = iter0->getSpeed() / 1024;
		std::ostringstream downloadSpeed_s;
		downloadSpeed_s << downloadSpeed << " kB/s";

		infoBuffer info;
		info.hash = iter0->getHash();

		//get all IP's
		for(std::list<download::serverElement *>::iterator iter1 = iter0->Server.begin(); iter1 != iter0->Server.end(); iter1++){
			info.server_IP += (*iter1)->server_IP + ", ";
		}

		if(info.server_IP.empty()){
			info.server_IP = "searching..";
		}

		info.fileName = iter0->getFileName();
		info.fileSize = fileSize_s.str();
		info.speed = downloadSpeed_s.str();
		info.percentComplete = percentComplete;

		//happens sometimes during testing or if something went wrong
		if(percentComplete > 100){
			info.percentComplete = 100;
		}

		downloadInfo.push_back(info);
	}

	}//end lock scope

	return true;
}

int client::getTotalSpeed()
{
	int speed = 0;

	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){
		speed += iter0->getSpeed();
	}

	}//end lock scope

	return speed;
}

bool client::newConnection(int & socketfd, std::string & server_IP)
{
	sockaddr_in dest_addr;
	socketfd = socket(PF_INET, SOCK_STREAM, 0);

	dest_addr.sin_family = AF_INET;                           //set socket type TCP
	dest_addr.sin_port = htons(global::P2P_PORT);             //set destination port
	dest_addr.sin_addr.s_addr = inet_addr(server_IP.c_str()); //set destination IP
	memset(&(dest_addr.sin_zero),'\0',8);                     //zero out the rest

	if(connect(socketfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1){
		return false; //new connection failed
	}
#ifdef DEBUG
	std::cout << "info: client::sendRequest() created socket " << socketfd << " for " << server_IP << "\n";
#endif

	//make sure fdmax is always the highest socket number
	if(socketfd > fdmax){
		fdmax = socketfd;

		{//begin lock scope
		boost::mutex::scoped_lock lock(sendBufferMutex);

		//resize send/receive buffers if necessary
		while(sendBuffer.size() <= fdmax){
			std::string newBuffer;
			sendBuffer.push_back(newBuffer);
		}

		sendBuffer[socketfd].reserve(global::C_CTRL_SIZE);

		}//end lock scope
	}

	FD_SET(socketfd, &masterfds);
	return true; //new connection created
}

inline void client::processBuffer(const int & socketfd, char recvBuff[], const int & nbytes)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){

		if(iter0->hasSocket(socketfd, nbytes)){

			iter0->processBuffer(socketfd, recvBuff, nbytes);
			if(iter0->complete()){
				iter0->startTerminate();
			}

			break;
		}
	}

	}//end lock scope
}

void client::resetHungDownloadSpeed()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){

		if(iter0->Server.empty()){
			iter0->zeroSpeed();
		}
	}

	}//end lock scope
}

bool client::removeAbusive_checkContains(std::list<download::abusiveServerElement> & abusiveServerTemp, download::serverElement * SE)
{
	for(std::list<download::abusiveServerElement>::iterator iter0 = abusiveServerTemp.begin(); iter0 != abusiveServerTemp.end(); iter0++){

		if(iter0->server_IP == SE->server_IP){
			return true;
		}
	}

	return false;
}

inline void client::removeAbusive()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	std::list<download::abusiveServerElement> abusiveServerTemp;

	//check to see if any downloads are reporting abusive servers
	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){
		abusiveServerTemp.splice(abusiveServerTemp.end(), iter0->abusiveServer);
	}

	//if new abusive servers discovered find and remove associated serverElements
	if(!abusiveServerTemp.empty()){

		//sort and remove duplicates
		abusiveServerTemp.sort();
		abusiveServerTemp.unique();
	
		//check for abusive serverElement pointers within each download
		for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){

			//remove serverElements marked as abusive
			std::list<download::serverElement *>::iterator Server_iter = iter0->Server.begin();
			while(Server_iter != iter0->Server.end()){

				if(removeAbusive_checkContains(abusiveServerTemp, *Server_iter)){
					Server_iter = iter0->Server.erase(Server_iter);
				}
				else{
					Server_iter++;
				}
			}
		}

		//check for abusive serverElement pointer rings
		std::list<std::list<download::serverElement *> >::iterator serverHolder_iter = serverHolder.begin();
		while(serverHolder_iter != serverHolder.end()){

			//if abusive element found remove ring and free ring memory
			if(removeAbusive_checkContains(abusiveServerTemp, serverHolder_iter->front())){
				disconnect(serverHolder_iter->front()->socketfd);

				for(std::list<download::serverElement *>::iterator iter0 = serverHolder_iter->begin(); iter0 != serverHolder_iter->end(); iter0++){
					delete *iter0;
				}

				serverHolder_iter = serverHolder.erase(serverHolder_iter);
			}
			else{
				serverHolder_iter++;
			}
		}
	}

	}//end lock scope

	resetHungDownloadSpeed();
}

inline void client::removeDisconnected(const int & socketfd)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);
	
	//find the disconnected serverElements in the downloads
	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){

		//remove serverElements that were disconnected
		std::list<download::serverElement *>::iterator Server_iter = iter0->Server.begin();
		while(Server_iter != iter0->Server.end()){

			if((*Server_iter)->socketfd == socketfd){
				Server_iter = iter0->Server.erase(Server_iter);
			}
			else{
				Server_iter++;
			}
		}
	}

	}//end lock scope

	{//begin lock scope
	boost::mutex::scoped_lock lock(serverHolderMutex);

	//find disconnected serverElement rings in the serverHolder
	std::list<std::list<download::serverElement *> >::iterator serverHolder_iter = serverHolder.begin();
	while(serverHolder_iter != serverHolder.end()){

		//remove serverElements rings for disconnected servers
		if(serverHolder_iter->front()->socketfd == socketfd){
			for(std::list<download::serverElement *>::iterator iter0 = serverHolder_iter->begin(); iter0 != serverHolder_iter->end(); iter0++){
				delete *iter0;
			}

			serverHolder_iter = serverHolder.erase(serverHolder_iter);
		}
		else{
			serverHolder_iter++;
		}
	}

	}//end lock scope

	resetHungDownloadSpeed();
}

inline void client::removeTerminated()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){

		if(iter0->readyTerminate()){
#ifdef DEBUG
			std::cout << "info: client::removeTerminated() deferred termination of \"" << iter0->getFileName() << "\" done\n";
#endif
			terminateDownload(iter0->getHash());
			break;
		}
	}

	}//end lock scope
}

inline void client::postResumeConnect()
{
	sockaddr_in dest_addr;

	//create sockets for the resumed downloads
	for(std::list<std::list<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

		int socketfd;
		newConnection(socketfd, iter0->front()->server_IP);
	
		//set all server elements with this IP to the same socket
		for(std::list<download::serverElement *>::iterator iter1 = iter0->begin(); iter1 != iter0->end(); iter1++){
			(*iter1)->socketfd = socketfd;
		}
	}

	resumeDownloads = false;
}

void client::start()
{
	boost::thread clientThread(boost::bind(&client::start_thread, this));
}

void client::start_thread()
{
	char recvBuff[global::BUFFER_SIZE];  //the receive buffer
   int nbytes;                          //how many bytes received in one shot

	FD_ZERO(&masterfds);

	//how long select() waits before returning(unless data received)
	timeval tv;

	//main client receive loop
	while(true){

		/*
		If the program was just started and it has downloads that need to be resumed this
		will connect to all the servers.
		*/
		if(resumeDownloads){
			postResumeConnect();
		}

		removeAbusive();    //remove servers marked as abusive
		removeTerminated(); //check for downloads scheduled for deletion
		prepareRequests();  //send requests for servers that are ready

		/*
		These must be initialized every iteration on linux(and possibly other OS's)
		because linux will change them(POSIX.1-2001 allows this). This work-around 
		doesn't adversely effect other operating systems.
		*/
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		/*
		This if(sendPending) exists to save a LOT of CPU time by not checking
		if sockets are ready to write when we don't need to write anything.
		*/
		if(sendPending){

			readfds = masterfds;
			writefds = masterfds;

			if((select(fdmax+1, &readfds, &writefds, NULL, &tv)) == -1){

				//gprof will send PROF signal, this will ignore it
				if(errno != EINTR){
					perror("select");
					exit(1);
				}
			}
		}
		else{

			FD_ZERO(&writefds);
			readfds = masterfds;

			if((select(fdmax+1, &readfds, NULL, NULL, &tv)) == -1){

				//gprof will send PROF signal, this will ignore it
				if(errno != EINTR){
					perror("select");
					exit(1);
				}
			}
		}

		//begin loop through all of the sockets, checking for flags
		for(int x=0; x<=fdmax; x++){

			if(FD_ISSET(x, &readfds)){
				if((nbytes = recv(x, recvBuff, sizeof(recvBuff), 0)) <= 0){
					removeDisconnected(x);
					disconnect(x);
				}
				else{
					processBuffer(x, recvBuff, nbytes);
				}
			}

			if(FD_ISSET(x, &writefds)){

				{//begin lock scope
				boost::mutex::scoped_lock lock(sendBufferMutex);

				if(!sendBuffer[x].empty()){

					if((nbytes = send(x, sendBuffer[x].c_str(), sendBuffer[x].size(), 0)) <= 0){
						disconnect(x);
					}
					else{ //remove bytes sent from buffer
						sendBuffer[x].erase(0, nbytes);
					}
				}

				}//end lock scope
			}
		}

		sendPending = false;
	}
}

inline void client::prepareRequests()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(serverHolderMutex);

	//pass tokens
	for(std::list<std::list<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

		//if the download has relinquished it's token, give the token to the next download
		if(iter0->front()->token == false){
			iter0->push_back(iter0->front());
			iter0->pop_front();
			iter0->front()->token = true;
		}
	}

	}//end lock scope

	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	//process sendQueue
	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){

		/*
		Don't send new requests from downloads that are in the process of terminating
		or complete.
		*/
		if(iter0->terminating() || iter0->complete()){
			continue;
		}

		for(std::list<download::serverElement *>::iterator iter1 = iter0->Server.begin(); iter1 != iter0->Server.end(); iter1++){

			if((*iter1)->bytesExpected == 0 && (*iter1)->token){

				if(sendBuffer[(*iter1)->socketfd].empty()){
#ifdef DEBUG_VERBOSE
					std::cout << "info: client::prepareRequests() IP: " << (*iter1)->server_IP << "\n";
#endif
					unsigned int request = iter0->getRequest();
					std::ostringstream request_s;
					request_s << global::P_SBL << encodeInt((*iter1)->file_ID) << encodeInt(request);

					sendBuffer[(*iter1)->socketfd] = request_s.str();

					//will be ready again when a response it received to this requst
					(*iter1)->blockRequested = request;
					(*iter1)->bytesExpected = global::BUFFER_SIZE;
					(*iter1)->lastRequest = false;

					/*
					If the last request was made to a server the serverElement must be marked
					so it knows to expect a incomplete packet.
					*/
					if(request == iter0->getLastBlock()){
						(*iter1)->lastRequest = true;
						(*iter1)->bytesExpected = iter0->getLastBlockSize();
					}

					sendPending = true;
				}
			}
		}
	}

	}//end lock scope
}

std::string client::encodeInt(unsigned int number)
{
	std::string encodedNumber;
	std::bitset<32> bs = number;
	std::bitset<32> bs_temp;

	for(int x=0; x<4; x++){
		bs_temp = bs;
		bs_temp <<= 8*x;
		bs_temp >>= 24;
		encodedNumber += (unsigned char)bs_temp.to_ulong();
	}

	return encodedNumber;
}

bool client::startDownload(exploration::infoBuffer info)
{
	//make sure file isn't already downloading
	if(!ClientIndex.startDownload(info)){
		return false;
	}

	std::string filePath;
	if(!ClientIndex.getFilePath(info.hash, filePath)){
		return 0;
	}

	int fileSize = atoi(info.fileSize.c_str());
	int blockCount = 0;
	int lastBlock = atoi(info.fileSize.c_str())/(global::BUFFER_SIZE - global::S_CTRL_SIZE);
	int lastBlockSize = atoi(info.fileSize.c_str()) % (global::BUFFER_SIZE - global::S_CTRL_SIZE) + global::S_CTRL_SIZE;
	int lastSuperBlock = lastBlock / global::SUPERBLOCK_SIZE;
	int currentSuperBlock = 0;

	download newDownload(
		info.hash,
		info.fileName,
		filePath,
		fileSize,
		blockCount,
		lastBlock,
		lastBlockSize,
		lastSuperBlock,
		currentSuperBlock
	);

	{//begin lock scope
	boost::mutex::scoped_lock lock(serverHolderMutex);

	//add known servers associated with this download
	for(int x=0; x<info.server_IP.size(); x++){

		download::serverElement * SE = new download::serverElement();
		SE->server_IP = info.server_IP[x];
		SE->file_ID = atoi(info.file_ID[x].c_str());

		//if a socket for this server already exists then reuse it
		bool socketFound = false;
		for(std::list<std::list<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

			if(iter0->front()->server_IP == SE->server_IP){
				SE->socketfd = iter0->front()->socketfd;
				socketFound = true;
				break;
			}
		}

		//if existing socket not found create a new one
		if(!socketFound){

			//if cannot connect to server don't add this element
			if(!newConnection(SE->socketfd, SE->server_IP)){
#ifdef DEBUG
				std::cout << "info: client::startDownload() could not connect to " << SE->server_IP << "\n";
#endif
				delete SE;
				continue;
			}
		}

		//add the server to appropriate server ring if it exists, if not create a new ring
		bool ringFound = false;
		for(std::list<std::list<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

			if(iter0->front()->server_IP == SE->server_IP){
				//all servers in the vector must have the same socket
				SE->socketfd = iter0->front()->socketfd;
				iter0->push_back(SE);
				ringFound = true;
			}
		}

		if(!ringFound){
			//make a new server deque for the server and add it to the serverHolder
			std::list<download::serverElement *> newServer;
			newServer.push_back(SE);
			serverHolder.push_back(newServer);
		}

		//add server to the download
		newDownload.Server.push_back(SE);
	}

	}//end lock scope

	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	downloadBuffer.push_back(newDownload);

	}//end lock scope

	return true;
}

void client::stopDownload(const std::string & hash)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	//find the download and set it to terminate
	std::list<download>::iterator download_iter;
	for(download_iter = downloadBuffer.begin(); download_iter != downloadBuffer.end(); download_iter++){

		if(hash == download_iter->getHash()){
			download_iter->startTerminate();
			break;
		}
	}

	if(download_iter->readyTerminate()){
#ifdef DEBUG
		std::cout << "info: client::stopDownload() doing quick terminate of \"" << download_iter->getFileName() << "\"\n";
#endif
		terminateDownload(hash);
	}
#ifdef DEBUG
	else{
		std::cout << "info: client::stopDownload() is scheduling the download to be removed\n";
	}
#endif

	}//end lock scope
}

//there are intentionally not locks in this function, check header file for information
void client::terminateDownload(const std::string & hash)
{
	//find the download to terminate
	std::list<download>::iterator download_iter;
	for(download_iter = downloadBuffer.begin(); download_iter != downloadBuffer.end(); download_iter++){

		if(hash == download_iter->getHash()){
			break;
		}
	}

	/*
	Find the serverHolder element and remove the serverElements from it that correspond
	to the serverElements in the download. If a socket is abandoned(no other downloads
	using it) then get rid of it.
	*/
	while(!download_iter->Server.empty()){

		//holder for socket to be disconnected if no other downloads are using it
		int socket;

		/*
		Look through the serverHolder rings for serverElements matching
		download_iter->Server.back(). When a matching element is found erase it
		from the ring, pop the back of download_iter->Server.
		*/
		for(std::list<std::list<download::serverElement *> >::iterator serverHolder_iter = serverHolder.begin(); serverHolder_iter != serverHolder.end(); serverHolder_iter++){
			for(std::list<download::serverElement *>::iterator ring_iter = serverHolder_iter->begin(); ring_iter != serverHolder_iter->end(); ring_iter++){

				if(*ring_iter == download_iter->Server.back()){
					socket = (*ring_iter)->socketfd;
					delete *ring_iter;
					ring_iter = serverHolder_iter->erase(ring_iter);
					download_iter->Server.pop_back();
				}
			}
		}

		/*
		There may be an empty server ring after the above double-for erased a
		serverElement from a ring. If there is that means there are no more
		downloads using the socket and the empty ring will be erased and the
		socket disconnected.
		*/
		std::list<std::list<download::serverElement *> >::iterator serverHolder_iter = serverHolder.begin();
		while(serverHolder_iter != serverHolder.end()){

			if(serverHolder_iter->empty()){
				disconnect(socket);
				serverHolder_iter = serverHolder.erase(serverHolder_iter);
			}
			else{
				serverHolder_iter++;
			}
		}
	}

	//remove download entry from the database
	ClientIndex.terminateDownload(download_iter->getHash());

	//get rid of the download from the downloadBuffer
	downloadBuffer.erase(download_iter);
}

