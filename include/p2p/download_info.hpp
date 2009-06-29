#ifndef H_DOWNLOAD_INFO
#define H_DOWNLOAD_INFO

//include
#include <boost/cstdint.hpp>

//standard
#include <string>
#include <vector>

class download_info
{
public:
	download_info(
		const std::string & hash_in,
		const std::string & name_in,
		const boost::uint64_t & size_in
	):
		hash(hash_in),
		name(name_in),
		size(size_in)
	{}

	std::vector<std::string> IP; //IP's of all servers that possibly have the file
	std::string hash;            //root hash of hash tree
	std::string name;            //name of the file
	boost::uint64_t size;        //size of the file (bytes)
};
#endif
