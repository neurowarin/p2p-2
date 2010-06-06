#include "block_request.hpp"

//BEGIN have_element
block_request::have_element::have_element(const boost::function<void()> & trigger_tick_in):
	trigger_tick(trigger_tick_in)
{

}

block_request::have_element::have_element(const have_element & HE):
	trigger_tick(HE.trigger_tick)
{

}
//END have_element

block_request::block_request(const boost::uint64_t block_count_in):
	block_count(block_count_in),
	local_blocks(0),
	local(block_count),
	approved(block_count)
{

}

void block_request::add_block_local(const boost::uint64_t block)
{
	boost::mutex::scoped_lock lock(Mutex);
	assert(Have.empty());
	if(!local.empty()){
		assert(local[block] == false);
		local[block] = true;
		++local_blocks;
		if(local.all_set()){
			local.clear();
		}
	}
}

void block_request::add_block_local(const int connection_ID, const boost::uint64_t block)
{
	boost::mutex::scoped_lock lock(Mutex);
	bool send_have = false;
	if(!local.empty()){
		if(local[block] == false){
			send_have = true;
		}
		local[block] = true;
		++local_blocks;
		if(local.all_set()){
			local.clear();
		}
	}
	request.erase(block);
	if(send_have){
		/*
		We don't tell the host that sent us a block that we have the block. We
		also don't tell it we have blocks we know it already has.
		*/
		std::set<int> trigger;
		for(std::map<int, have_element>::iterator it_cur = Have.begin(),
			it_end = Have.end(); it_cur != it_end; ++it_cur)
		{
			if(it_cur->first != connection_ID){
				std::map<int, bit_field>::iterator iter = remote.find(it_cur->first);
				if(iter == remote.end()){
					/*
					We are downloading from remote host, but it is not downoading from
					us so we don't know what blocks it has.
					*/
					it_cur->second.block.push(block);
					trigger.insert(it_cur->first);
				}else{
					/*
					We are downloading from remote host, so we can check to see if it
					already has a block. If it does there's no need to tell it we have
					the block because it won't need to request it from us.
					*/
					if(iter->second.empty() || iter->second[block] == false){
						//remote host doesn't have block
						it_cur->second.block.push(block);
						trigger.insert(it_cur->first);
					}
				}
			}
		}
		for(std::map<int, have_element>::iterator it_cur = Have.begin(),
			it_end = Have.end(); it_cur != it_end; ++it_cur)
		{
			if(it_cur->first != connection_ID && trigger.find(it_cur->first) != trigger.end()){
				it_cur->second.trigger_tick();
			}
		}
	}
}

void block_request::add_block_local_all()
{
	boost::mutex::scoped_lock lock(Mutex);
	local.clear();
	request.clear();
	local_blocks = block_count;
}

void block_request::add_block_remote(const int connection_ID,
	const boost::uint64_t block)
{
	boost::mutex::scoped_lock lock(Mutex);
	//find bitset for remote host
	std::map<int, bit_field>::iterator remote_iter = remote.find(connection_ID);
	assert(remote_iter != remote.end());
	//add block
	if(!remote_iter->second.empty()){
		remote_iter->second[block] = true;
		if(remote_iter->second.all_set()){
			remote_iter->second.clear();
		}
	}
}

void block_request::approve_block(const boost::uint64_t block)
{
	boost::mutex::scoped_lock lock(Mutex);
	if(!approved.empty()){
		approved[block] = true;
		if(approved.all_set()){
			approved.clear();
		}
	}
}

void block_request::approve_block_all()
{
	boost::mutex::scoped_lock lock(Mutex);
	approved.clear();
}

boost::uint64_t block_request::bytes()
{
	return block_count % 8 == 0 ? block_count / 8 : block_count / 8 + 1;
}

bool block_request::complete()
{
	boost::mutex::scoped_lock lock(Mutex);
	/*
	When the dynamic_bitset is complete it is clear()'d to save space. If a
	bit_field is clear we know the host has all blocks.
	*/
	return local.empty();
}

unsigned block_request::download_count()
{
	boost::mutex::scoped_lock lock(Mutex);
	return remote.size();
}

void block_request::download_subscribe(const int connection_ID,
	const bit_field & BF)
{
	boost::mutex::scoped_lock lock(Mutex);
	assert(BF.empty() || BF.size() == block_count);
	std::pair<std::map<int, bit_field>::iterator, bool>
		ret = remote.insert(std::make_pair(connection_ID, BF));
	assert(ret.second);
}

void block_request::download_unsubscribe(const int connection_ID)
{
	boost::mutex::scoped_lock lock(Mutex);
	remote.erase(connection_ID);
	//erase request elements for this host
	std::map<boost::uint64_t, std::set<int> >::iterator
		it_cur = request.begin(), it_end = request.end();
	while(it_cur != it_end){
		it_cur->second.erase(connection_ID);
		if(it_cur->second.empty()){
			request.erase(it_cur++);
		}else{
			++it_cur;
		}
	}
}

boost::optional<boost::uint64_t> block_request::find_next_rarest(const int connection_ID)
{
	//find bitset for remote host
	std::map<int, bit_field>::iterator remote_iter = remote.find(connection_ID);
	if(remote_iter == remote.end()){
		//we are waiting on a bit_field from the remote host most likely
		return boost::optional<boost::uint64_t>();
	}
	boost::uint64_t rare_block;           //most rare block
	boost::uint64_t rare_block_hosts = 0; //number of hosts that have rare_block
	for(boost::uint64_t block = 0; block < local.size(); ++block){
		if(local[block]){
			//we already have this block
			continue;
		}
		if(!approved.empty() && !approved[block]){
			//block is not approved
			continue;
		}
		if(!remote_iter->second.empty() && remote_iter->second[block] == false){
			//remote host doesn't have this block
			continue;
		}

		//check rarity
		boost::uint32_t hosts = 0;
		for(std::map<int, bit_field>::iterator it_cur = remote.begin(),
			it_end = remote.end(); it_cur != it_end; ++it_cur)
		{
			if(it_cur->second.empty() || it_cur->second[block]){
				++hosts;
			}
		}
		if(hosts == 1){
			//block with maximum rarity found
			if(request.find(block) == request.end()){
				//block not already requested
				return block;
			}
		}else if(hosts < rare_block_hosts || rare_block_hosts == 0){
			//a new most-rare block found
			if(request.find(block) == request.end()){
				//block not already requested, consider requesting this block
				rare_block = block;
				rare_block_hosts = hosts;
			}
		}
	}
	if(rare_block_hosts == 0){
		//host has no blocks we need
		return boost::optional<boost::uint64_t>();
	}else{
		//a block was found that we need
		return rare_block;
	}
}

bool block_request::have_block(const boost::uint64_t block)
{
	boost::mutex::scoped_lock lock(Mutex);
	assert(block < block_count);
	if(local.empty()){
		//complete
		return true;
	}else{
		return local[block] == true;
	}
}

bool block_request::is_approved(const boost::uint64_t block)
{
	boost::mutex::scoped_lock lock(Mutex);
	if(approved.empty()){
		return true;
	}else{
		return approved[block] == true;
	}
}

boost::optional<boost::uint64_t> block_request::next_have(const int connection_ID)
{
	boost::mutex::scoped_lock lock(Mutex);
	std::map<int, have_element>::iterator iter = Have.find(connection_ID);
	if(iter == Have.end()){
		return boost::optional<boost::uint64_t>();
	}else{
		if(iter->second.block.empty()){
			return boost::optional<boost::uint64_t>();
		}else{
			boost::uint64_t block = iter->second.block.front();
			iter->second.block.pop();
			return block;
		}
	}
}

boost::optional<boost::uint64_t> block_request::next_request(const int connection_ID)
{
	boost::mutex::scoped_lock lock(Mutex);
	if(local.empty()){
		//complete
		return boost::optional<boost::uint64_t>();
	}
	/*
	At this point we know there are no timed out requests to the host and there
	are no re-requests to do. We move on to checking for the next rarest block to
	request from the host.
	*/
	if(boost::optional<boost::uint64_t> block = find_next_rarest(connection_ID)){
		//there is a new block to request
		std::pair<std::map<boost::uint64_t, std::set<int> >::iterator, bool>
			r_ret = request.insert(std::make_pair(*block, std::set<int>()));
		assert(r_ret.second);
		std::pair<std::set<int>::iterator, bool> c_ret = r_ret.first->second.insert(connection_ID);
		assert(c_ret.second);
		return block;
	}else{
		/*
		No new blocks to request. If this connection has no requests pending then
		determine what block has been requested least, and make duplicate request.
		*/

		//determine if we have any requests pending
		for(std::map<boost::uint64_t, std::set<int> >::iterator
			it_cur = request.begin(), it_end = request.end();
			it_cur != it_end; ++it_cur)
		{
			if(it_cur->second.find(connection_ID) != it_cur->second.end()){
				//pending request found, make no duplicate request
				return boost::optional<boost::uint64_t>();
			}
		}

		//find the least requested block that the remote host has and request it
		std::map<int, bit_field>::iterator remote_iter = remote.find(connection_ID);
		boost::uint64_t rare_block;
		unsigned min_request = std::numeric_limits<unsigned>::max();
		for(std::map<boost::uint64_t, std::set<int> >::iterator
			it_cur = request.begin(), it_end = request.end();
			it_cur != it_end; ++it_cur)
		{
			if(it_cur->second.size() == 1){
				//least requested possible, check if remote host has it
				if(remote_iter->second.empty() || remote_iter->second[it_cur->first] == true){
					it_cur->second.insert(connection_ID);
					return it_cur->first;
				}
			}else if(it_cur->second.size() < min_request){
				//new least requested block found, check if remote host has it
				if(remote_iter->second.empty() || remote_iter->second[it_cur->first] == true){
					rare_block = it_cur->first;
					min_request = it_cur->second.size();
				}
			}
		}
		if(min_request != std::numeric_limits<unsigned>::max()){
			std::map<boost::uint64_t, std::set<int> >::iterator iter = request.find(rare_block);
			assert(iter != request.end());
			iter->second.insert(connection_ID);
			return rare_block;
		}else{
			return boost::optional<boost::uint64_t>();
		}
	}
}

unsigned block_request::percent_complete()
{
	boost::mutex::scoped_lock lock(Mutex);
	if(local.empty()){
		return 100;
	}else{
		return ((double)local_blocks / block_count) * 100;
	}
}

unsigned block_request::remote_host_count()
{
	boost::mutex::scoped_lock lock(Mutex);
	return remote.size();
}

unsigned block_request::upload_count()
{
	boost::mutex::scoped_lock lock(Mutex);
	return Have.size();
}

bit_field block_request::upload_subscribe(const int connection_ID,
	const boost::function<void()> trigger_tick)
{
	boost::mutex::scoped_lock lock(Mutex);
	//add have element
	std::pair<std::map<int, have_element>::iterator, bool>
		p = Have.insert(std::make_pair(connection_ID, have_element(trigger_tick)));
	assert(p.second);
	return local;
}

void block_request::upload_unsubscribe(const int connection_ID)
{
	boost::mutex::scoped_lock lock(Mutex);
	Have.erase(connection_ID);
}
