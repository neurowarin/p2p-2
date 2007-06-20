#include <iostream>
#include <fstream>
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

void client::disconnect(int socketfd)
{
#ifdef DEBUG
	std::cout << "info: client disconnecting socket number " << socketfd << "\n";
#endif

	//if socket = -1 then there is no corresponding socket in masterfds
	if(socketfd != -1){
		close(socketfd);

		{//begin lock scope
		boost::mutex::scoped_lock lock(masterfdsMutex);
		FD_CLR(socketfd, &masterfds);
		}//end lock scope
	}
}

bool client::getDownloadInfo(std::vector<infoBuffer> & downloadInfo)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	if(downloadBuffer.size() == 0){
		return false;
	}

	//process sendQueue
	for(std::vector<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){
		//calculate the percent complete
		float bytesDownloaded = iter0->getBlockCount() * (global::BUFFER_SIZE - global::CONTROL_SIZE);
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
		for(std::vector<download::serverElement *>::iterator iter1 = iter0->Server.begin(); iter1 != iter0->Server.end(); iter1++){
			info.server_IP += (*iter1)->server_IP + ", ";
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

std::string client::getTotalSpeed()
{
	int speed = 0;

	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	for(std::vector<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){
		speed += iter0->getSpeed();
	}

	}//end lock scope

	speed = speed / 1024; //convert to kB
	std::ostringstream speed_s;
	speed_s << speed << " kB/s";

	return speed_s.str();
}

void client::processBuffer(int socketfd, char recvBuff[], int nbytes)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	for(std::vector<download>::iterator iter0 = downloadBuffer.begin(); iter0 != downloadBuffer.end(); iter0++){

		if(iter0->hasSocket(socketfd, nbytes)){

			iter0->processBuffer(socketfd, recvBuff, nbytes);
			if(iter0->complete()){
				terminateDownload_real(iter0->getMessageDigest());
			}

			break;
		}
	}

	}//end lock scope
}

void client::postResumeConnect()
{
	sockaddr_in dest_addr;

	//create sockets for the resumed downloads
	for(std::vector<std::deque<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){
			
		int socketfd = socket(PF_INET, SOCK_STREAM, 0);
		FD_SET(socketfd, &masterfds);

		dest_addr.sin_family = AF_INET;                           //set socket type TCP
		dest_addr.sin_port = htons(global::P2P_PORT);             //set destination port
		dest_addr.sin_addr.s_addr = inet_addr(iter0->front()->server_IP.c_str()); //set destination IP
		memset(&(dest_addr.sin_zero),'\0',8);                     //zero out the rest of struct

		//make sure fdmax is always the highest socket number
		if(socketfd > fdmax){
			fdmax = socketfd;
		}

		if(connect(socketfd, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) == -1){
#ifdef DEBUG
			std::cout << "error: client::sendRequest() could not connect to server\n";
#endif
		}
#ifdef DEBUG
		std::cout << "info: client::sendRequest() created socket " << socketfd << " for " << iter0->front()->server_IP << "\n";
#endif

		//set all server elements with this IP to the same socket
		for(std::deque<download::serverElement *>::iterator iter1 = iter0->begin(); iter1 != iter0->end(); iter1++){
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

	//how long select() waits before returning(unless data received)
	timeval tv;

	//main client receive loop
	while(true){

		if(resumedDownloads){
			postResumeConnect();
		}

		sendPendingRequests();

		/*
		These must be initialized every iteration on linux(and possibly other OS's)
		because linux will change them(POSIX.1-2001 allows this). This work-around 
		doesn't adversely effect other operating systems.
		*/
		tv.tv_sec = 1;
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
					disconnect(x);
				}
				else{
					processBuffer(x, recvBuff, nbytes);
				}
			}
		}
	}
}

void client::sendPendingRequests()
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	//pass tokens
	for(std::vector<std::deque<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

		//if the download has relinquished it's token, give the token to the next download
		if(iter0->front()->token == false){
			iter0->push_back(iter0->front());
			iter0->pop_front();
			iter0->front()->token = true;
		}
	}

	//process sendQueue
	for(std::vector<download>::iterator iter0 = downloadBuffer.begin(); iter0 < downloadBuffer.end(); iter0++){
		for(std::vector<download::serverElement *>::iterator iter1 = iter0->Server.begin(); iter1 != iter0->Server.end(); iter1++){

			if((*iter1)->bytesExpected == 0 && (*iter1)->token){
#ifdef DEBUG_VERBOSE
				std::cout << "info: client::sendPendingRequests() IP: " << (*iter1)->server_IP << " ready: " << (*iter1)->ready << "\n";
#endif
				int request = iter0->getRequest();
				sendRequest((*iter1)->server_IP, (*iter1)->socketfd, (*iter1)->file_ID, request);

				//will be ready again when a response it received to this requst
				(*iter1)->bytesExpected = global::BUFFER_SIZE;

				/*
				If the last request was made to a server the serverElement must be marked
				so it knows to expect a incomplete packet.
				*/
				if(request == iter0->getLastBlock()){
					(*iter1)->lastRequest = true;
				}
			}
		}
	}

	}//end lock scope
}

int client::sendRequest(std::string server_IP, int & socketfd, int file_ID, int fileBlock)
{
	sockaddr_in dest_addr;

	//if no socket then create one
	if(socketfd == -1){
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
			return 0;
		}
#ifdef DEBUG
		std::cout << "info: client::sendRequest() created socket " << socketfd << " for " << server_IP << "\n";
#endif

	}

	std::ostringstream sendRequest_s;
	sendRequest_s << global::P_SBL << "," << file_ID << "," << fileBlock << ",";

	int bytesToSend = sendRequest_s.str().length();
	int nbytes = 0; //how many bytes sent in one shot

	//send request
	while(bytesToSend > 0){
		nbytes = send(socketfd, sendRequest_s.str().c_str(), sendRequest_s.str().length(), 0);

		if(nbytes == -1){
#ifdef DEBUG
			std::cout << "error: client::sendRequest() had error on sending request\n";
#endif
			break;
		}

		bytesToSend -= nbytes;
	}

#ifdef DEBUG_VERBOSE
	std::cout << "info: client sending " << sendRequest_s.str() << " to " << server_IP << "\n";
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
	int lastBlock = atoi(info.fileSize_bytes.c_str())/(global::BUFFER_SIZE - global::CONTROL_SIZE);
	int lastBlockSize = atoi(info.fileSize_bytes.c_str()) % (global::BUFFER_SIZE - global::CONTROL_SIZE) + global::CONTROL_SIZE;
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
		SE->socketfd = -1; //will be changed to appropriate number if socket already connected to server

		//see if a socket for this server already exists, if it does then reuse it
		for(std::vector<std::deque<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

			if(iter0->front()->server_IP == info.server_IP.at(x)){
				SE->socketfd = iter0->front()->socketfd;
				break;
			}
		}

		//add the server to the server vector if it exists, otherwise make a new one and add it to serverHolder
		bool found = false;
		for(std::vector<std::deque<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

			if(iter0->front()->server_IP == SE->server_IP){
				//all servers in the vector must have the same socket
				SE->socketfd = iter0->front()->socketfd;
				iter0->push_back(SE);
				found = true;
			}
		}

		if(!found){
			//make a new server deque for the server and add it to the serverHolder
			std::deque<download::serverElement *> newServer;
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

void client::terminateDownload(std::string messageDigest_in)
{
	{//begin lock scope
	boost::mutex::scoped_lock lock(downloadBufferMutex);

	terminateDownload_real(messageDigest_in);

	}//end lock scope
}

void client::terminateDownload_real(std::string messageDigest_in)
{
	//find the download
	std::vector<download>::iterator download_iter;
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
		for(std::vector<std::deque<download::serverElement *> >::iterator iter0 = serverHolder.begin(); iter0 != serverHolder.end(); iter0++){

			for(std::deque<download::serverElement *>::iterator iter1 = iter0->begin(); iter1 != iter0->end(); iter1++){

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
		for(std::vector<std::deque<download::serverElement *> >::iterator serverHolder_iter = serverHolder.begin(); serverHolder_iter != serverHolder.end(); serverHolder_iter++){

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

