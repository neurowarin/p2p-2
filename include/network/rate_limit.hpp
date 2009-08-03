/*
This class handles rate limiting and can be used to get access to the average
upload and download rates. It is also used to set maximum upload/download rates.
This class is generally accessed through the reactor interface.
*/
//THREADSAFE
#ifndef H_NETWORK_RATE_LIMIT
#define H_NETWORK_RATE_LIMIT

//include
#include <boost/thread.hpp>
#include <speed_calculator.hpp>

//standard
#include <limits>

namespace network{
class rate_limit
{
public:
	rate_limit():
		_max_download_rate(std::numeric_limits<unsigned>::max()),
		_max_upload_rate(std::numeric_limits<unsigned>::max())
	{}

	//add n_bytes to download rate
	void add_download_bytes(const unsigned n_bytes)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		Download.add(n_bytes);
	}

	//add n_bytes to upload rate
	void add_upload_bytes(const unsigned n_bytes)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		Upload.add(n_bytes);
	}

	/*
	Returns number of bytes that can be sent such that we don't go over
	_max_upload_rate.
	*/
	int available_upload()
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		unsigned transfer = 0;
		if(Upload.current_second() >= _max_upload_rate){
			//limit reached, send no bytes
			return transfer;
		}else{
			//limit not yet reached, determine how many bytes to send
			transfer = _max_upload_rate - Upload.current_second();
			if(transfer > std::numeric_limits<int>::max()){
				return std::numeric_limits<int>::max();
			}else{
				return transfer;
			}
		}
	}

	/*
	Returns number of bytes that can be received such that we don't go over
	_max_download_rate.
	*/
	int available_download()
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		unsigned transfer = 0;
		if(Download.current_second() >= _max_download_rate){
			//limit reached, send no bytes
			return transfer;
		}else{
			//limit not yet reached, determine how many bytes to send
			transfer = _max_download_rate - Download.current_second();
			if(transfer > std::numeric_limits<int>::max()){
				return std::numeric_limits<int>::max();
			}else{
				return transfer;
			}
		}
	}

	//returns average current download rate
	unsigned current_download_rate()
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		return Download.speed();
	}

	//returns average current upload rate
	unsigned current_upload_rate()
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		return Upload.speed();
	}

	//returns maximum download rate
	unsigned max_download_rate()
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		return _max_download_rate;
	}

	//returns maximum upload rate
	unsigned max_upload_rate()
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		return _max_upload_rate;
	}

	//sets maximum download rate
	void max_download_rate(const unsigned rate)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		if(rate == 0){
			_max_download_rate = 1;
		}else{
			_max_download_rate = rate;
		}
	}

	//sets maximum upload rate
	void max_upload_rate(const unsigned rate)
	{
		boost::recursive_mutex::scoped_lock lock(Recursive_Mutex);
		if(rate == 0){
			_max_upload_rate = 1;
		}else{
			_max_upload_rate = rate;
		}
	}

private:
	//mutex for all static public functions
	boost::recursive_mutex Recursive_Mutex;

	unsigned _max_download_rate;
	unsigned _max_upload_rate;

	speed_calculator Download;
	speed_calculator Upload;
};
}//end of network namespace
#endif
