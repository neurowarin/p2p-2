#include "p2p_real.hpp"

p2p_real::p2p_real():
	Proactor(
		boost::bind(&connection_manager::connect_call_back, &Connection_Manager, _1),
		boost::bind(&connection_manager::disconnect_call_back, &Connection_Manager, _1),
		settings::P2P_PORT
	),
	Connection_Manager(Proactor),
	Thread_Pool(1),
	max_connections_proxy(0),
	max_download_rate_proxy(0),
	max_upload_rate_proxy(0)
{
	resume_thread = boost::thread(boost::bind(&p2p_real::resume, this));
}

p2p_real::~p2p_real()
{
	Thread_Pool.interrupt_join();
	resume_thread.interrupt();
	resume_thread.join();
}

unsigned p2p_real::download_rate()
{
	return Proactor.download_rate();
}

unsigned p2p_real::max_connections()
{
	return max_connections_proxy;
}

void p2p_real::max_connections(unsigned connections)
{
//DEBUG, add support for this
	//Proactor.max_connections(connections / 2, connections / 2);
	max_connections_proxy = connections;
	Thread_Pool.queue(boost::bind(&database::table::preferences::set_max_connections,
		connections, database::pool::get()));
}

unsigned p2p_real::max_download_rate()
{
	return max_download_rate_proxy;
}

void p2p_real::max_download_rate(const unsigned rate)
{
	Proactor.max_download_rate(rate);
	max_download_rate_proxy = rate;
	Thread_Pool.queue(boost::bind(&database::table::preferences::set_max_download_rate,
		rate, database::pool::get()));
}

unsigned p2p_real::max_upload_rate()
{
	return max_upload_rate_proxy;
}

void p2p_real::max_upload_rate(const unsigned rate)
{
	Proactor.max_upload_rate(rate);
	max_upload_rate_proxy = rate;
	Thread_Pool.queue(boost::bind(&database::table::preferences::set_max_upload_rate,
		rate, database::pool::get()));
}

void p2p_real::pause_download(const std::string & hash)
{

}

unsigned p2p_real::prime_count()
{
	return prime_generator::singleton().prime_count();
}

void p2p_real::resume()
{
	//delay helps GUI have responsive startup
	boost::this_thread::yield();

	//setup proxies
	max_connections_proxy = database::table::preferences::get_max_connections();
	max_download_rate_proxy = database::table::preferences::get_max_download_rate();
	max_upload_rate_proxy = database::table::preferences::get_max_upload_rate();

	//set prefs with proactor
	Proactor.max_download_rate(max_download_rate_proxy);
	Proactor.max_upload_rate(max_upload_rate_proxy);

	//repopulate share from database
	std::deque<database::table::share::info> resume = database::table::share::resume();
	while(!resume.empty()){
		file_info FI(
			resume.front().hash,
			resume.front().path,
			resume.front().file_size,
			resume.front().last_write_time
		);
		share::singleton().insert(FI);
		if(resume.front().file_state == database::table::share::downloading){
			//trigger slot creation for downloading file
			share::singleton().find_slot(FI.hash);
		}
		resume.pop_front();
	}

	//hash check resumed downloads
	for(share::slot_iterator iter_cur = share::singleton().begin_slot(),
		iter_end = share::singleton().end_slot(); iter_cur != iter_end; ++iter_cur)
	{
		iter_cur->check();
	}

	//start share scanning after share repopulated
	Share_Scanner.start();

	//get host_info for all hosts we need to connect to
	std::set<std::pair<std::string, std::string> > all_host;
	for(share::slot_iterator iter_cur = share::singleton().begin_slot(),
		iter_end = share::singleton().end_slot(); iter_cur != iter_end; ++iter_cur)
	{
		iter_cur->merge_host(all_host);
	}

	//connect to all hosts that have files we need
	for(std::set<std::pair<std::string, std::string> >::iterator iter_cur = all_host.begin(),
		iter_end = all_host.end(); iter_cur != iter_end; ++iter_cur)
	{
		Proactor.connect(iter_cur->first, iter_cur->second, network::tcp);
	}

	//bring up networking
	Proactor.start();
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

}

void p2p_real::remove_download(const std::string & hash)
{

}

void p2p_real::transfers(std::vector<p2p::transfer> & T)
{
	T.clear();
	for(share::slot_iterator iter_cur = share::singleton().begin_slot(),
		iter_end = share::singleton().end_slot(); iter_cur != iter_end; ++iter_cur)
	{
		p2p::transfer transfer;
		transfer.hash = iter_cur->hash();
		transfer.name = iter_cur->name();
		transfer.file_size = iter_cur->file_size();
		transfer.percent_complete = iter_cur->percent_complete();
		transfer.upload_speed = iter_cur->upload_speed();
		transfer.download_speed = iter_cur->download_speed();
		T.push_back(transfer);
	}
}

unsigned p2p_real::upload_rate()
{
	return Proactor.upload_rate();
}
