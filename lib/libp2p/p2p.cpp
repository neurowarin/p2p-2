//custom
#include "p2p_impl.hpp"

//include
#include <p2p.hpp>
#include <portable.hpp>

//BEGIN download_info
p2p::download_info::download_info(
	const std::string & hash_in,
	const std::string & name_in
):
	hash(hash_in),
	name(name_in)
{

}
//END download_info

p2p::p2p():
	P2P_impl(new p2p_impl())
{

}

unsigned p2p::connections()
{
	return P2P_impl->connections();
}

unsigned p2p::DHT_count()
{
	return P2P_impl->DHT_count();
}

unsigned p2p::get_max_connections()
{
	return P2P_impl->get_max_connections();
}

unsigned p2p::get_max_download_rate()
{
	return P2P_impl->get_max_download_rate();
}

unsigned p2p::get_max_upload_rate()
{
	return P2P_impl->get_max_upload_rate();
}

void p2p::load_file(const std::string & path)
{
	path::create_dirs();

	boost::regex expr(".+\\.p2p");
	if(!boost::regex_match(path.begin(), path.end(), expr)){
		//not *.p2p file, don't try to load
		return;
	}

	//to be atomic we copy the file to the tmp directory then move it
	std::stringstream tmp_path, load_path;
	tmp_path << path::tmp_dir() << "load_" << portable::getpid() << ".p2p";
	load_path << path::load_dir() << "load_" << portable::getpid() << ".p2p";
	std::fstream fin(path.c_str(), std::ios::in | std::ios::binary);
	if(fin.is_open()){
		std::fstream fout(tmp_path.str().c_str(), std::ios::out | std::ios::binary);
		fout << fin.rdbuf();
		std::rename(tmp_path.str().c_str(), load_path.str().c_str());
	}
}

void p2p::remove_download(const std::string & hash)
{
	P2P_impl->remove_download(hash);
}

void p2p::set_db_file_name(const std::string & name)
{
	path::set_db_file_name(name);
}

void p2p::set_max_connections(const unsigned connections)
{
	P2P_impl->set_max_connections(connections);
}

void p2p::set_max_download_rate(const unsigned rate)
{
	P2P_impl->set_max_download_rate(rate);
}

void p2p::set_max_upload_rate(const unsigned rate)
{
	P2P_impl->set_max_upload_rate(rate);
}

void p2p::set_program_dir(const std::string & path)
{
	path::set_program_dir(path);
}

boost::uint64_t p2p::share_size()
{
	return P2P_impl->share_size();
}

boost::uint64_t p2p::share_files()
{
	return P2P_impl->share_files();
}

void p2p::start_download(const p2p::download_info & DI)
{
	P2P_impl->start_download(DI);
}

std::list<p2p::transfer_info> p2p::transfer()
{
	return P2P_impl->transfer();
}

boost::optional<p2p::transfer_info> p2p::transfer(const std::string & hash)
{
	return P2P_impl->transfer(hash);
}

unsigned p2p::TCP_download_rate()
{
	return P2P_impl->TCP_download_rate();
}

unsigned p2p::TCP_upload_rate()
{
	return P2P_impl->TCP_upload_rate();
}

unsigned p2p::UDP_download_rate()
{
	return P2P_impl->UDP_download_rate();
}

unsigned p2p::UDP_upload_rate()
{
	return P2P_impl->UDP_upload_rate();
}
