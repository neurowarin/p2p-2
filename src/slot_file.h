#ifndef H_SLOT_FILE
#define H_SLOT_FILE

//custom
#include "slot.h"

//std
#include <string>

class slot_file: public slot
{
public:
	slot_file(
		std::string * IP_in,
		const std::string & root_hash_in,
		const boost::uint64_t & file_size_in,
		const std::string & path_in
	);

	/*
	Documentation in slot.h
	*/
	virtual void send_block(const std::string & request, std::string & send);
	void info(std::vector<upload_info> & UI);

private:
	//path to file the slot is for
	std::string path;

	//buffer for reading file blocks
	char send_block_buff[global::FILE_BLOCK_SIZE];

	//total number of blocks in file (used for percent calculation)
	boost::uint64_t block_count;

	//percent complete uploading (0-100)
	unsigned int percent;
	boost::uint64_t highest_block_seen;

	/*
	udpate_percent - calculates percent complete
	                 latest_block: the most recent block requested
	*/
	void update_percent(const boost::uint64_t & latest_block);
};
#endif
