#ifndef H_P2P
#define H_P2P

//include
#include <boost/utility.hpp>
#include <boost/shared_ptr.hpp>

//standard
#include <string>
#include <vector>

class p2p_real;
class p2p : private boost::noncopyable
{
public:
	p2p();

	//status of a transfer
	class transfer
	{
	public:
		std::string hash;          //root hash of hash tree
		std::string name;          //name of the file
		boost::uint64_t file_size; //size of the file (bytes)
		boost::uint64_t tree_size; //size of hash tree (bytes)
		unsigned percent_complete; //0-100
		unsigned download_speed;   //total download bytes/second
		unsigned upload_speed;     //total upload bytes/second

		//host associated with current speed
		std::vector<std::pair<std::string, unsigned> > upload;
		std::vector<std::pair<std::string, unsigned> > download;
	};

	//needed to start a download
	class download
	{
	public:
		download(
			const std::string & hash_in,
			const std::string & name_in,
			const boost::uint64_t & file_size_in
		):
			hash(hash_in),
			name(name_in),
			file_size(file_size_in)
		{}

		std::string hash;              //root hash of hash tree
		std::string name;              //name of the file
		boost::uint64_t file_size;     //size of the file (bytes)
		std::vector<std::string> host; //hosts known to have this file
	};

	/*
	download_rate:
		Returns current download rate (excluding localhost).
	max_connections:
		Get or set connection limit.
	max_download_rate:
		Get or set download rate limit (bytes).
	max_upload_rate:
		Get or set upload rate limit (bytes).
	pause_download:
		Pause a download.
	prime_count:
		Returns number of primes in prime cache used for key-exchanges.
	remove_download:
		Removes a running download.
	share_size_bytes:
		Size of all shared files.
	share_size_files:
		The number of files shared.
	start_download:
		Starts a download.
	transfers:
		Clears T and populates it with transfer information.
	upload_rate:
		Returns current upload rate (excluding localhost).
	*/
	unsigned download_rate();
	unsigned max_connections();
	void max_connections(const unsigned connections);
	unsigned max_download_rate();
	void max_download_rate(const unsigned rate);
	unsigned max_upload_rate();
	void max_upload_rate(const unsigned rate);
	void pause_download(const std::string & hash);
	unsigned prime_count();
	void remove_download(const std::string & hash);
	boost::uint64_t share_size_bytes();
	boost::uint64_t share_size_files();
	void start_download(const p2p::download & D);
	void transfers(std::vector<p2p::transfer> & T);
	unsigned upload_rate();

private:
	boost::shared_ptr<p2p_real> P2P_Real;
};
#endif
