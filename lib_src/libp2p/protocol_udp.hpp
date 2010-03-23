#ifndef H_PROTOCOL_UDP
#define H_PROTOCOL_UDP

//include
#include <SHA1.hpp>

namespace protocol_udp
{
//kademlia
const unsigned bucket_count = SHA1::bin_size * 8;
const unsigned bucket_size = 20;
const unsigned timeout = 65;

//commands and message sizes
const unsigned char ping = 0;
const unsigned ping_size = 9;
const unsigned char pong = 1;
const unsigned pong_size = 29;
}//end of namespace protocol_udp
#endif
