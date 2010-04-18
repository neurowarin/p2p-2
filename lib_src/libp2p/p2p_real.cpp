#include "p2p_real.hpp"

p2p_real::p2p_real()
{
	LOG << "port: " << db::table::prefs::get_port() << " peer_ID: "
		<< db::table::prefs::get_ID();
	resume_thread = boost::thread(boost::bind(&p2p_real::resume, this));
}

p2p_real::~p2p_real()
{
	resume_thread.interrupt();
	resume_thread.join();
}

unsigned p2p_real::download_rate()
{
	return Connection_Manager.Proactor.download_rate();
}

unsigned p2p_real::max_connections()
{
	return db::table::prefs::get_max_connections();
}

void p2p_real::max_connections(unsigned connections)
{
//DEBUG, add support for this
	//Proactor.max_connections(connections / 2, connections / 2);
	db::table::prefs::set_max_connections(connections);
}

unsigned p2p_real::max_download_rate()
{
	return db::table::prefs::get_max_download_rate();
}

void p2p_real::max_download_rate(const unsigned rate)
{
	Connection_Manager.Proactor.max_download_rate(rate);
	db::table::prefs::set_max_download_rate(rate);
}

unsigned p2p_real::max_upload_rate()
{
	return db::table::prefs::get_max_upload_rate();
}

void p2p_real::max_upload_rate(const unsigned rate)
{
	Connection_Manager.Proactor.max_upload_rate(rate);
	db::table::prefs::set_max_upload_rate(rate);
}

void p2p_real::resume()
{
	db::table::prefs::warm_up_cache();

	//delay helps GUI have responsive startup
	boost::this_thread::yield();

	//set prefs with proactor
	max_download_rate(db::table::prefs::get_max_download_rate());
	max_upload_rate(db::table::prefs::get_max_upload_rate());

	//repopulate share from database
	std::deque<db::table::share::info> resume = db::table::share::resume();
	while(!resume.empty()){
		file_info FI(
			resume.front().hash,
			resume.front().path,
			resume.front().file_size,
			resume.front().last_write_time
		);
		share::singleton().insert(FI);
		if(resume.front().file_state == db::table::share::downloading){
			//trigger slot creation for downloading file
			share::singleton().find_slot(FI.hash);
		}
		resume.pop_front();
	}

	//start scanning share
	Share_Scanner.start();

	//hash check resumed downloads
	for(share::slot_iterator iter_cur = share::singleton().begin_slot(),
		iter_end = share::singleton().end_slot(); iter_cur != iter_end; ++iter_cur)
	{
		if(iter_cur->get_transfer()){
			iter_cur->get_transfer()->check();
		}
	}

	//connect to peers that have files we need
	std::set<std::pair<std::string, std::string> >
		peers = db::table::join::resume_peers();
	for(std::set<std::pair<std::string, std::string> >::iterator
		iter_cur = peers.begin(), iter_end = peers.end(); iter_cur != iter_end;
		++iter_cur)
	{
		Connection_Manager.Proactor.connect(iter_cur->first, iter_cur->second);
	}

	//bring up networking
	std::set<network::endpoint> E = network::get_endpoint(
		"",
		db::table::prefs::get_port(),
		network::tcp
	);
	assert(!E.empty());
	if(!Connection_Manager.Proactor.start(*E.begin())){
		LOG << "stub: handle failed listener start";
		exit(1);
	}
}

boost::uint64_t p2p_real::share_size_bytes()
{
	return share::singleton().bytes();
}

boost::uint64_t p2p_real::share_size_files()
{
	return share::singleton().files();
}

void p2p_real::start_download(const p2p::download & D)
{
	LOG;
}

void p2p_real::remove_download(const std::string & hash)
{
	LOG << hash;
/*
	Thread_Pool.enqueue(boost::bind(&connection_manager::remove,
		&Connection_Manager, hash));
*/
}

void p2p_real::transfers(std::vector<p2p::transfer> & T)
{
	T.clear();
	for(share::slot_iterator iter_cur = share::singleton().begin_slot(),
		iter_end = share::singleton().end_slot(); iter_cur != iter_end; ++iter_cur)
	{
		iter_cur->touch();

		p2p::transfer transfer;
		transfer.hash = iter_cur->hash();
		transfer.name = iter_cur->name();
		transfer.file_size = iter_cur->file_size();
		transfer.percent_complete = iter_cur->percent_complete();
		transfer.download_peers = iter_cur->download_peers();
		transfer.download_speed = iter_cur->download_speed();
		transfer.upload_peers = iter_cur->upload_peers();
		transfer.upload_speed = iter_cur->upload_speed();
		T.push_back(transfer);
	}
}

unsigned p2p_real::upload_rate()
{
	return Connection_Manager.Proactor.upload_rate();
}
