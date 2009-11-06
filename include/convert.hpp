#ifndef H_CONVERT
#define H_CONVERT

//include
#include <boost/cstdint.hpp>
#include <boost/detail/endian.hpp>
#include <logger.hpp>

//standard
#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <string>

namespace convert {

//returns num encoded in big-endian
template<class T>
static std::string encode(const T & num)
{
	union{
		T n;
		char b[sizeof(T)];
	} NB;
	NB.n = num;
	#ifdef BOOST_LITTLE_ENDIAN
	std::reverse(NB.b, NB.b+sizeof(T));
	#endif
	return std::string(NB.b, sizeof(T));
}

/*
Returns num encoded in big-endian. However only returns enough bytes to hold
range of numbers [0, end). We call this a VLI (variable length interger).
Note: This only works with unsigned integers.
*/
static std::string encode_VLI(const boost::uint64_t & num,
	const boost::uint64_t & end)
{
	assert(num < end);
	//determine index of first used byte for max
	int index = 0;
	std::string temp = encode(end);
	for(; index<temp.size(); ++index){
		if(temp[index] != 0){
			break;
		}
	}
	//return bytes for value
	temp = encode(num);
	return temp.substr(index);
}

//decode number encoded in big-endian
template<class T>
static T decode(const std::string & encoded)
{
	assert(encoded.size() == sizeof(T));
	union{
		T n;
		char b[sizeof(T)];
	} NB;
	#ifdef BOOST_LITTLE_ENDIAN
	std::reverse_copy(encoded.data(), encoded.data()+sizeof(T), NB.b);
	#elif
	std::copy(encoded.data(), encoded.data()+sizeof(T), NB.b);
	#endif
	return NB.n;
}

//decode VLI
static boost::uint64_t decode_VLI(std::string encoded)
{
	assert(encoded.size() > 0 && encoded.size() <= 8);
	//prepend zero bytes if needed
	encoded = std::string(8 - encoded.size(), '\0') + encoded;
	return decode<boost::uint64_t>(encoded);
}

//returns the size of string encode_VLI would return given the specified end
static unsigned VLI_size(const boost::uint64_t & end)
{
	assert(end > 0);
	unsigned index = 0;
	std::string temp = encode(end);
	for(; index<temp.size(); ++index){
		if(temp[index] != 0){
			return temp.size() - index;
		}
	}
	LOGGER; exit(1);
}

/*
Returns binary version of hex string encoded in big-endian. Returns empty string
if invalid hex string. The following types of strings are invalid.
1. Empty string.
2. String with lower case hex chars (all hex chars must be upper case).
3. String with length not multiple of 2.
*/
static std::string hex_to_bin(const char * hex, const int size)
{
	std::string bin;

	//check for invalid size
	if(size == 0 || size % 2 != 0){
		return bin;
	}

	for(int x=0; x<size; x+=2){
		//make sure hex characters are valid
		if(!(hex[x] >= '0' && hex[x] <= '9' || hex[x] >= 'A' && hex[x] <= 'F')
			|| !(hex[x+1] >= '0' && hex[x+1] <= '9' || hex[x+1] >= 'A' && hex[x+1] <= 'F'))
		{
			bin.clear();
			return bin;
		}

		//convert
		char ch = (hex[x] >= 'A' ? hex[x] - 'A' + 10 : hex[x] - '0') << 4;
		bin += ch + (hex[x+1] >= 'A' ? hex[x+1] - 'A' + 10 : hex[x+1] - '0');
	}
	return bin;
}

static std::string hex_to_bin(const std::string & hex)
{
	return hex_to_bin(hex.data(), hex.size());
}

//converts a binary string to hex
static std::string bin_to_hex(const char * bin, const int size)
{
	const char * const hex = "0123456789ABCDEF";
	std::string temp;
	for(int x=0; x<size; ++x){
		temp += hex[static_cast<int>((bin[x] >> 4) & 15)];
		temp += hex[static_cast<int>(bin[x] & 15)];
	}
	return temp;
}

//converts a binary string to hex
static std::string bin_to_hex(const std::string & bin)
{
	return bin_to_hex(bin.data(), bin.size());
}

//convert bytes to reasonable SI unit, example 1024 -> 1kB
static std::string size_SI(const boost::uint64_t & bytes)
{
	std::ostringstream oss;
	if(bytes < 1024){
		oss << bytes << "B";
	}else if(bytes >= 1024ull*1024*1024*1024){
		oss << std::fixed << std::setprecision(2)
			<< bytes / static_cast<double>(1024ull*1024*1024*1024) << "tB";
	}else if(bytes >= 1024*1024*1024){
		oss << std::fixed << std::setprecision(2)
			<< bytes / static_cast<double>(1024*1024*1024) << "gB";
	}else if(bytes >= 1024*1024){
		oss << std::fixed << std::setprecision(2)
			<< bytes / static_cast<double>(1024*1024) << "mB";
	}else if(bytes >= 1024){
		oss << std::fixed << std::setprecision(1)
			<< bytes / static_cast<double>(1024) << "kB";
	}
	return oss.str();
}

/*
Given two strings obtained from size_unit_select, returns less than (-1),
equal to (0), or greater than (1). Just like strcmp.

This function doesn't do any converting but it's related to size_SI so it
goes in the convert namespace.

Note: This function only looks for the beginning of the size unit so it is ok to
pass it strings obtained from size_SI that have had additional characters
appended to them. For example it is ok to compare 5kB/s to 10kB/s.
*/
static int size_SI_cmp(const std::string & left, const std::string & right)
{
	//size prefix used for comparison
	const char * prefix = "Bkmgt";

	int left_prefix = -1;
	for(int x=0; x<left.size(); ++x){
		if(!((int)left[x] >= 48 && (int)left[x] <= 57) && left[x] != '.'){
			for(int y=0; y<5; ++y){
				if(left[x] == prefix[y]){
					left_prefix = y;
					goto end_left_find;
				}
			}
		}
	}
	end_left_find:

	int right_prefix = -1;
	for(int x=0; x<right.size(); ++x){
		if(!((int)right[x] >= 48 && (int)right[x] <= 57) && right[x] != '.'){
			for(int y=0; y<5; ++y){
				if(right[x] == prefix[y]){
					right_prefix = y;
					goto end_right_find;
				}
			}
		}
	}
	end_right_find:

	if(left_prefix == -1 || right_prefix == -1){
		return 0;
	}else if(left_prefix < right_prefix){
		return -1;
	}else if(right_prefix < left_prefix){
		return 1;
	}else{
		//same units on both sides, compare values
		std::string left_temp(left, 0, left.find(prefix[left_prefix]));
		std::string right_temp(right, 0, right.find(prefix[right_prefix]));
		left_temp.erase(std::remove(left_temp.begin(), left_temp.end(), '.'),
			left_temp.end());
		right_temp.erase(std::remove(right_temp.begin(), right_temp.end(), '.'),
			right_temp.end());
		return std::strcmp(left_temp.c_str(), right_temp.c_str());		
	}
}
}//end of namespace convert
#endif
