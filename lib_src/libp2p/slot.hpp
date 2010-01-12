#ifndef H_SLOT
#define H_SLOT

//custom
#include "database.hpp"
#include "file_info.hpp"
#include "transfer.hpp"

//include
#include <atomic_bool.hpp>
#include <boost/cstdint.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>

//standard
#include <string>
#include <vector>

class share;
class slot : private boost::noncopyable
{
	/*
	Slots are unique (there exists only one slot per hash). Only the share class
	is able to instantiate a slot. This makes it easy to enforce uniqueness.
	*/
	friend class share;

public:
	/* Info
	complete:
		Returns true if both the Hash_Tree and File are complete.
	download_peers:
		Returns how many hosts we're downloading this file from.
	download_speed:
		Returns download speed (bytes/second).
	file_size:
		Returns file size (bytes). If returned file size is 0 it means we don't
		yet know the file size.
	hash:
		Returns hash file is tracked by.
	name:
		Returns the name of the file.
	path:
		Returns full path to file slot represents.
	percent_complete:
		Returns percent complete of the download (0-100).
	upload_peers:
		Returns how many hosts we're uploading this file to.
	upload_speed:
		Returns upload speed (bytes/second).
	*/
	bool complete();
	unsigned download_peers();
	unsigned download_speed();
	boost::uint64_t file_size();
	const std::string & hash();
	const std::string & name();
	std::string path();
	unsigned percent_complete();
	unsigned upload_peers();
	unsigned upload_speed();

	/* Other
	get_transfer:
		Get object responsible for exchanging hash tree and file data.
		Note: May be empty shared_ptr if we don't yet know the file size.
	register_download/upload:
		Called open slot opened with remote host. Used to keep track of how many
		hosts we're downloading from/uploading to.
		Note: register_download called when outgoing slot opened.
		Note: register_upload called when incoming slot opened.
	set_unknown:
		Sets data that may not be known. Instantiates transfer. Returns false if
		transfer could not be instantiated.
	touch:
		Updates speeds. Called before getting speeds to return to the GUI.
	unregister_download/upload:
		Called when slot closed with remote host. Used to keep track of how many
		hosts we're downloading from/uploading to.
		Note: unregister_download called when outgoing slot closed.
		Note: unregister_upload called when incoming slot closed.
	*/
	const boost::shared_ptr<transfer> & get_transfer();
	void register_download();
	void register_upload();
	bool set_unknown(const int connection_ID, const boost::uint64_t file_size,
		const std::string & root_hash);
	void touch();
	void unregister_download();
	void unregister_upload();

private:
	/*
	Note: The ctor may throw.
	Note: If FI_in.file_size = 0 it means we don't know the file size.
	*/
	slot(const file_info & FI_in);

	const file_info FI;
	const std::string file_name;

	//instantiated when we get file size
	boost::mutex Transfer_mutex; //lock for instantiation of transfer
	boost::shared_ptr<transfer> Transfer;

	//counts for how many peers downloading/uploading
	atomic_int<unsigned> downloading;
	atomic_int<unsigned> uploading;
};
#endif
