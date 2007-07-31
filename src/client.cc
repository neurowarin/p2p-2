//std
#include <algorithm>
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

	//no mutex needed because client thread can't be running yet
	resumedDownloads = ClientIndex.initialFillBuffer(downloadBuffer, serverHolder);
}

inline void client::disconnect(int socketfd)
{
#ifdef DEBUG
	std::cout << "info: client disconnecting socket number " << socketfd << "\n";
#endif

	{//begin lock scope
	boost::mutex::scoped_lock lock(masterfdsMutex);

	close(socketfd);
	FD_CLR(socketfd, &masterfds);

	//reduce fdmax if possible
	int maxFound = 0;
	for(int x=0; x<fdmax; x++){
		if(FD_ISSET(x, &masterfds)){
			maxFound = x;
		}
	}

	fdmax = maxFound;

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
		info.messageDigest = iter0->getMessageDigest();

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
	FD_SET(socketfd, &masterfds);

	dest_addr.sin_family = AF_INET;                           //set socket type TCP
	dest_addr.sin_port = htons(global::P2P_PORT);             //set destination port
	dest_addr.sin_addr.s_addr = inet_addr(server_IP.c_str()); //set destination IP
	memset(&(dest_addr.sin_zero),'\0',8);                     //zero out the rest of struct

	//make sure fdmax is always the highest socket number
	if(socketfd > fdmax){
		fdmax = socketfd;
	}

	if(connect(socketfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1){
#ifdef DEBUG
		std::cout << "error: client::sendRequest() could not connect to server\n";
#endif
		return false; //new connection failed
	}
#ifdef DEBUG
	std::cout << "info: client::sendRequest() created socket " << socketfd << " for " << server_IP << "\n";
#endif

	return true; //new connection created
}

inline void client::processBuffer(int socketfd, char recvBuff[], int nbytes)
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
	for(std::list<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){

		if(iter0->Server.empty()){
			iter0->zeroSpeed();
		}
	}
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

	resetHungDownloadSpeed();

	}//end lock scope
}

inline void client::removeDisconnected(int socketfd)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);
	
	//check for disconnected serverElements
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

	//check for disconnected serverElements
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

	resetHungDownloadSpeed();

	}//end lock scope
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
			terminateDownload(iter0->getMessageDigest());
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

	resumedDownloads = false;
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
		if(resumedDownloads){
			postResumeConnect();
		}

		removeAbusive();       //remove servers marked as abusive
		removeTerminated();    //check for downloads scheduled for deletion
		sendPendingRequests(); //send requests for servers that are ready

		/*
		These must be initialized every iteration on linux(and possibly other OS's)
		because linux will change them(POSIX.1-2001 allows this). This work-around 
		doesn't adversely effect other operating systems.
		*/
		tv.tv_sec = 2;
		tv.tv_usec = 0;

		/*
		select() changes the fd_set passed to it to only reflect the sockets that need
		to be read. Because of this a copy must be kept(masterfds) so select() doesn't
		blow away the set. Before every call on select() readfds must be set equal to
		masterfds so that select will check all sockets to see if they're ready for a
		read. If they are they'll be left in the set, if they're not they'll be removed
		from the set.
		*/
		{//begin lock scope
		boost::mutex::scoped_lock lock(masterfdsMutex);
		readfds = masterfds;
		}//end lock scope

		if((select(fdmax+1, &readfds, NULL, NULL, &tv)) == -1){
			perror("select");
			exit(1);
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
		}
	}
}

inline void client::sendPendingRequests()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	//pass tokens
	for(std::list<std::list<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

		//if the download has relinquished it's token, give the token to the next download
		if(iter0->front()->token == false){
			iter0->push_back(iter0->front());
			iter0->pop_front();
			iter0->front()->token = true;
		}
	}

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
#ifdef DEBUG_VERBOSE
				std::cout << "info: client::sendPendingRequests() IP: " << (*iter1)->server_IP << " ready: " << (*iter1)->ready << "\n";
#endif
				int request = iter0->getRequest();
				sendRequest((*iter1)->server_IP, (*iter1)->socketfd, (*iter1)->file_ID, request);

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
			}
		}
	}

	}//end lock scope
}

int client::sendRequest(std::string server_IP, int & socketfd, int file_ID, int fileBlock)
{
	std::ostringstream request_s;
	request_s << global::P_SBL << global::DELIMITER << file_ID << global::DELIMITER << fileBlock << global::DELIMITER;

	//the request has to be REQUEST_CONTROL_SIZE bytes exactly
	std::string request = request_s.str();
	request.append(global::REQUEST_CONTROL_SIZE - request.length(), ' '); //blank the extra space

	int bytesToSend = request_s.str().length();
	int nbytes = 0; //how many bytes sent in one shot

	//send request
	while(bytesToSend > 0){
		nbytes = send(socketfd, request.c_str(), request.length(), 0);

		if(nbytes == -1){
#ifdef DEBUG
			std::cout << "error: client::sendRequest() had error on sending request\n";
#endif
			break;
		}

		bytesToSend -= nbytes;
	}

#ifdef DEBUG_VERBOSE
	std::cout << "info: client sending " << request << " to " << server_IP << "\n";
#endif

	return 0;
}

bool client::startDownload(exploration::infoBuffer info)
{
	//make sure file isn't already downloading
	if(ClientIndex.startDownload(info) < 0){
		return false;
	}

	/*
	This could be done with less copying but this function only gets run once when
	a download starts and this way is much more clear and self-documenting.
	*/
	std::string filePath = ClientIndex.getFilePath(info.messageDigest);
	int fileSize = atoi(info.fileSize_bytes.c_str());
	int blockCount = 0;
	int lastBlock = atoi(info.fileSize_bytes.c_str())/(global::BUFFER_SIZE - global::RESPONSE_CONTROL_SIZE);
	int lastBlockSize = atoi(info.fileSize_bytes.c_str()) % (global::BUFFER_SIZE - global::RESPONSE_CONTROL_SIZE) + global::RESPONSE_CONTROL_SIZE;
	int lastSuperBlock = lastBlock / global::SUPERBLOCK_SIZE;
	int currentSuperBlock = 0;

	download newDownload(
		info.messageDigest,
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
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	//add known servers associated with this download
	for(int x=0; x<info.server_IP.size(); x++){

		download::serverElement * SE = new download::serverElement();
		SE->server_IP = info.server_IP[x];
		SE->file_ID = atoi(info.file_ID[x].c_str());

		//see if a socket for this server already exists, if it does then reuse it
		bool socketFound = false;
		for(std::list<std::list<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

			if(iter0->front()->server_IP == info.server_IP[x]){
				SE->socketfd = iter0->front()->socketfd;
				break;
			}
		}

		//if existing socket not found create a new one
		if(!socketFound){

			//if cannot connect to server don't add this element
			if(!newConnection(SE->socketfd, SE->server_IP)){
				continue;
			}
		}

		//add the server to the server vector if it exists, otherwise make a new one and add it to serverHolder
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

	downloadBuffer.push_back(newDownload);

	}//end lock scope

	return true;
}

void client::stopDownload(std::string messageDigest_in)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	//find the download and set it to terminate
	std::list<download>::iterator download_iter;
	for(download_iter = downloadBuffer.begin(); download_iter != downloadBuffer.end(); download_iter++){

		if(messageDigest_in == download_iter->getMessageDigest()){
			download_iter->startTerminate();
			break;
		}
	}

	if(download_iter->readyTerminate()){
#ifdef DEBUG
		std::cout << "client::stopDownload() doing quick terminate of \"" << download_iter->getFileName() << "\"\n";
#endif
		terminateDownload(messageDigest_in);
	}
#ifdef DEBUG
	else{
		std::cout << "info: client::stopDownload() is scheduling the download to be removed\n";
	}
#endif

	}//end lock scope
}

void client::terminateDownload(std::string messageDigest_in)
{
	//find the download
	std::list<download>::iterator download_iter;
	for(download_iter = downloadBuffer.begin(); download_iter != downloadBuffer.end(); download_iter++){

		if(messageDigest_in == download_iter->getMessageDigest()){
			break;
		}
	}

	/*
	Find the serverHolder element and remove the serverElements from it that correspond
	to the serverElements in the download. If a socket is abandoned(no downloads using
	it) then disconnect it.
	*/
	while(!download_iter->Server.empty()){

		int socketfd;

		/*
		Erase the serverElement pointer that exists in both the serverHolder and
		within the download Server container. Free the memory for it.
		*/
		bool foundElement = false; //true if iterator was invalidated
		for(std::list<std::list<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

			for(std::list<download::serverElement *>::iterator iter1 = iter0->begin(); iter1 != iter0->end(); iter1++){

				if(*iter1 == download_iter->Server.back()){

					/*
					Store the socket file descriptor found in this Server element for
					the purpose of disconnecting it later(if no other downloads are
					using it.
					*/
					socketfd = (*iter1)->socketfd;

					delete *iter1;
					iter0->erase(iter1);
					download_iter->Server.pop_back();
					break;
				}
			}

			if(foundElement){
				break;
			}
		}

		//if there is an empty serverHolder deque element after the removal above then erase it
		for(std::list<std::list<download::serverElement *> >::iterator serverHolder_iter = serverHolder.begin(); serverHolder_iter != serverHolder.end(); serverHolder_iter++){

			if(serverHolder_iter->empty()){

				/*
				If the serverHolder has an empty element that indicates that the last
				serverElement removed from it belonged to the last download using the
				socket. If this is the case then disconnect the socket.
				*/
				disconnect(socketfd);

				//get rid of the empty serverHolder element
				serverHolder.erase(serverHolder_iter);
				break;
			}
		}
	}

	//remove download from download index file
	ClientIndex.terminateDownload(download_iter->getMessageDigest());

	//get rid of the downlaod from the downloadBuffer
	downloadBuffer.erase(download_iter);
}

