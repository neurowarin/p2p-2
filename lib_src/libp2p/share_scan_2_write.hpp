//THREADSAFE, CTOR THREAD SPAWNING
/*
This class is designed the encapsulate a thread which combines updating the
share and hash table in to large transactions.

Rationale: Updates to the share and hash table can be very frequent when
large directories are removed or directories with lots of small files are added.
Without combining the updates in to large transactions there is an unreasonable
amount of database contention.
*/
#ifndef H_SHARE_SCAN_2_WRITE
#define H_SHARE_SCAN_2_WRITE

//custom
#include "share.hpp"
#include "share_scan_job.hpp"
#include "share_scan_1_hash.hpp"

//include
#include <boost/bind.hpp>
#include <boost/thread.hpp>

//standard
#include <vector>

class share_scan_2_write
{
public:
	share_scan_2_write(share & Shared_in);
	~share_scan_2_write();

	/*
	block_until_resumed:
		Blocks until share is fully populated with information in the database.
	*/
	void block_until_resumed();

private:
	boost::thread write_thread;

	//contains files in share
	share & Share;

	//previous stage in pipeline
	share_scan_1_hash Share_Scan_1_Hash;

	/*
	main_loop:
		Database write combining.
	*/
	void main_loop();
};
#endif
