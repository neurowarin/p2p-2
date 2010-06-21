#ifndef H_PRIME_GENERATOR
#define H_PRIME_GENERATOR

//custom
#include "db_all.hpp"
#include "protocol_tcp.hpp"
#include "settings.hpp"

//include
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/utility.hpp>
#include <mpa.hpp>
#include <RC4.hpp>
#include <thread_pool.hpp>

//standard
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <list>

class prime_generator : public singleton_base<prime_generator>
{
	friend class singleton_base<prime_generator>;
public:
	~prime_generator();

	/*
	random_prime:
		Returns random prime of size settings::DH_KEY_SIZE.
	*/
	mpa::mpint random_prime();

private:
	prime_generator();

	boost::thread_group Workers;

	/*
	mutex:
		Locks the Prime_Cache container.
	cond:
		Notified when a prime removed from the cache.
	*/
	boost::mutex Mutex;
	boost::condition_variable_any Cond;
	std::list<mpa::mpint> Cache;

	/*
	generate:
		Generates a prime and puts it in the cache.
		Worker threads loop in this thread and generate primes.
	*/
	void generate();

	//random source used for prime generation is a lot of IO
	thread_pool_IO TP_IO;
};
#endif
