#include "slot_manager.hpp"

slot_manager::slot_manager(
	exchange_tcp & Exchange_in,
	const boost::function<void(const net::endpoint & ep, const std::string & hash)> & peer_call_back_in,
	const boost::function<void(const int)> & trigger_tick_in
):
	Exchange(Exchange_in),
	peer_call_back(peer_call_back_in),
	trigger_tick(trigger_tick_in),
	outgoing_pipeline_size(0),
	incoming_pipeline_size(0),
	open_slots(0),
	latest_slot(0)
{
	//register possible incoming messages
	Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(
		new message_tcp::recv::request_slot(boost::bind(
			&slot_manager::recv_request_slot, this, _1))));
	Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(
		new message_tcp::recv::close_slot(boost::bind(
			&slot_manager::recv_close_slot, this, _1))));
	Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(
		new message_tcp::recv::peer(boost::bind(
			&slot_manager::recv_peer, this, _1, _2))));
}

slot_manager::~slot_manager()
{
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Upload_Slot.begin(), it_end = Upload_Slot.end();
		it_cur != it_end; ++it_cur)
	{
		if(it_cur->second->get_transfer()){
			it_cur->second->get_transfer()->upload_unreg(Exchange.connection_ID);
		}
	}
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Download_Slot.begin(), it_end = Download_Slot.end();
		it_cur != it_end; ++it_cur)
	{
		if(it_cur->second->get_transfer()){
			it_cur->second->get_transfer()->download_unreg(Exchange.connection_ID);
		}
	}
	share::singleton().garbage_collect();
}

void slot_manager::add(const std::string & hash)
{
	if(Hash_Pending.find(hash) == Hash_Pending.end()
		&& Hash_Opened.find(hash) == Hash_Opened.end())
	{
		Hash_Pending.insert(hash);
	}
}

void slot_manager::close_complete()
{
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Download_Slot.begin(); it_cur != Download_Slot.end();)
	{
		if(it_cur->second->get_transfer()
			&& it_cur->second->get_transfer()->complete())
		{
			if(it_cur->second->get_transfer()){
				it_cur->second->get_transfer()->download_unreg(Exchange.connection_ID);
			}
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(
				new message_tcp::send::close_slot(it_cur->first)));
			Download_Slot.erase(it_cur++);
			share::singleton().garbage_collect();
		}else{
			++it_cur;
		}
	}
}

bool slot_manager::empty()
{
	return open_slots == 0 && Upload_Slot.empty() && Hash_Pending.empty();
}

bool slot_manager::recv_close_slot(const unsigned char slot_num)
{
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Upload_Slot.find(slot_num);
	if(it != Upload_Slot.end()){
		LOG << "close slot " << slot_num;
		if(it->second->get_transfer()){
			it->second->get_transfer()->upload_unreg(Exchange.connection_ID);
		}
		Upload_Slot.erase(it);
		return true;
	}else{
		LOG << "slot already closed";
		return false;
	}
}

bool slot_manager::recv_file_block(const net::buffer & block,
	const unsigned char slot_num, const boost::uint64_t block_num)
{
	--outgoing_pipeline_size;
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Download_Slot.find(slot_num);
	if(it != Download_Slot.end()){
		assert(it->second->get_transfer());
		transfer::status status = it->second->get_transfer()->write_file_block(
			Exchange.connection_ID, block_num, block);
		if(status == transfer::good){
			return true;
		}else if(status == transfer::bad){
			LOG << "error writing file, closing slot";
			if(it->second->get_transfer()){
				it->second->get_transfer()->download_unreg(Exchange.connection_ID);
			}
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(
				new message_tcp::send::close_slot(it->first)));
			Download_Slot.erase(it);
			share::singleton().garbage_collect();
		}else if(status == transfer::protocol_violated){
			LOG << "block arrived late";
		}
	}
	return true;
}

bool slot_manager::recv_have_file_block(const unsigned char slot_num,
	const boost::uint64_t block_num)
{
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Download_Slot.find(slot_num);
	if(it != Download_Slot.end()){
		if(it->second->get_transfer()){
			it->second->get_transfer()->recv_have_file_block(
				Exchange.connection_ID, block_num);
		}
	}
	return true;
}

bool slot_manager::recv_have_hash_tree_block(const unsigned char slot_num,
	const boost::uint64_t block_num)
{
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Download_Slot.find(slot_num);
	if(it != Download_Slot.end()){
		if(it->second->get_transfer()){
			it->second->get_transfer()->recv_have_hash_tree_block(
				Exchange.connection_ID, block_num);
		}
	}
	return true;
}

bool slot_manager::recv_hash_tree_block(const net::buffer & block,
	const unsigned char slot_num, const boost::uint64_t block_num)
{
	--outgoing_pipeline_size;
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Download_Slot.find(slot_num);
	if(it != Download_Slot.end()){
		assert(it->second->get_transfer());
		transfer::status status = it->second->get_transfer()->write_tree_block(
			Exchange.connection_ID, block_num, block);
		if(status == transfer::good){
			return true;
		}else if(status == transfer::bad){
			LOG << "error writing hash tree, closing slot";
			if(it->second->get_transfer()){
				it->second->get_transfer()->download_unreg(Exchange.connection_ID);
			}
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(
				new message_tcp::send::close_slot(it->first)));
			Download_Slot.erase(it);
			share::singleton().garbage_collect();
		}else if(status == transfer::protocol_violated){
			LOG << "block arrived late";
		}
	}
	return true;
}

bool slot_manager::recv_peer(const unsigned char slot_num,
	const net::endpoint & ep)
{
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Download_Slot.find(slot_num);
	if(it != Download_Slot.end()){
		peer_call_back(ep, it->second->hash());
	}
}

bool slot_manager::recv_request_block_failed(const unsigned char slot_num)
{
	LOG;
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Download_Slot.find(slot_num);
	if(it != Download_Slot.end()){
		if(it->second->get_transfer()){
			it->second->get_transfer()->download_unreg(Exchange.connection_ID);
		}
		Download_Slot.erase(it);
	}
	return true;
}

bool slot_manager::recv_request_hash_tree_block(const unsigned char slot_num,
	const boost::uint64_t block_num)
{
	if(incoming_pipeline_size >= protocol_tcp::max_block_pipeline){
		LOG << "overpipelined";
		return false;
	}
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Upload_Slot.find(slot_num);
	if(it == Upload_Slot.end()){
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(new message_tcp::send::error()));
	}else{
		assert(it->second->get_transfer());
		std::pair<net::buffer, transfer::status>
			p = it->second->get_transfer()->read_tree_block(block_num);
		if(p.second == transfer::good){
			++incoming_pipeline_size;
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(
				new message_tcp::send::block(p.first,
				it->second->get_transfer()->upload_speed_composite(Exchange.connection_ID))),
				boost::bind(&slot_manager::sent_block, this));
			return true;
		}else if(p.second == transfer::bad){
			LOG << "error reading tree block";
			it->second->get_transfer()->upload_unreg(Exchange.connection_ID);
			Upload_Slot.erase(it);
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(new message_tcp::send::error()));
			return true;
		}else{
			LOG << "violated protocol";
			return false;
		}
	}
	return true;
}

bool slot_manager::recv_request_file_block(const unsigned char slot_num,
	const boost::uint64_t block_num)
{
	if(incoming_pipeline_size >= protocol_tcp::max_block_pipeline){
		LOG << "overpipelined";
		return false;
	}
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it = Upload_Slot.find(slot_num);
	if(it == Upload_Slot.end()){
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(new message_tcp::send::error()));
	}else{
		assert(it->second->get_transfer());
		std::pair<net::buffer, transfer::status>
			p = it->second->get_transfer()->read_file_block(block_num);
		if(p.second == transfer::good){
			++incoming_pipeline_size;
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(
				new message_tcp::send::block(p.first,
				it->second->get_transfer()->upload_speed_composite(Exchange.connection_ID))),
				boost::bind(&slot_manager::sent_block, this));
			return true;
		}else if(p.second == transfer::bad){
			LOG << "error reading file block";
			it->second->get_transfer()->upload_unreg(Exchange.connection_ID);
			Upload_Slot.erase(it);
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(new message_tcp::send::error()));
			return true;
		}else{
			LOG << "violated protocol";
			return false;
		}
	}
	return true;
}

bool slot_manager::recv_request_slot(const std::string & hash)
{
	LOG << convert::abbr(hash);
	//locate requested slot
	share::slot_iterator slot_it = share::singleton().find_slot(hash);
	if(slot_it == share::singleton().end_slot()){
		LOG << "failed " << hash;
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(new message_tcp::send::error()));
		return true;
	}

	if(!slot_it->get_transfer()){
		//we do not yet know root_hash/file_size
		LOG << "failed " << convert::abbr(hash);
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(new message_tcp::send::error()));
		return true;
	}

	//find available slot number
	unsigned char slot_num = 0;
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Upload_Slot.begin(), it_end = Upload_Slot.end();
		it_cur != it_end; ++it_cur)
	{
		if(it_cur->first != slot_num){
			break;
		}
		++slot_num;
	}

	//bit_fields for tree and file
	assert(remote_listen);
	transfer::local_BF LBF = slot_it->get_transfer()->upload_reg(
		Exchange.connection_ID, *remote_listen, boost::bind(trigger_tick, Exchange.connection_ID));

	//file size
	boost::uint64_t file_size = slot_it->get_transfer()->file_size();
	if(file_size == 0){
		LOG << "failed " << convert::abbr(hash);
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::error()));
		return true;
	}

	//root hash
	boost::optional<std::string> root_hash = slot_it->get_transfer()->root_hash();
	if(!root_hash){
		LOG << "failed " << convert::abbr(hash);
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::error()));
		return true;
	}

	//we have all information to send slot message
	Exchange.send(boost::shared_ptr<message_tcp::send::base>(
		new message_tcp::send::slot(slot_num, file_size, *root_hash, LBF.tree_BF, LBF.file_BF)));

	//add slot
	std::pair<std::map<unsigned char, boost::shared_ptr<slot> >::iterator, bool>
		ret = Upload_Slot.insert(std::make_pair(slot_num, slot_it.get()));
	assert(ret.second);

	//unexpect any previous
	Exchange.expect_anytime_erase(boost::shared_ptr<message_tcp::send::base>(
		new message_tcp::send::request_hash_tree_block(slot_num, 0, 1)));
	Exchange.expect_anytime_erase(boost::shared_ptr<message_tcp::send::base>(
		new message_tcp::send::request_file_block(slot_num, 0, 1)));

	//expect incoming block requests
	Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(
		new message_tcp::recv::request_hash_tree_block(
		boost::bind(&slot_manager::recv_request_hash_tree_block, this, _1, _2),
		slot_num, slot_it->get_transfer()->tree_block_count())));
	Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(
		new message_tcp::recv::request_file_block(
		boost::bind(&slot_manager::recv_request_file_block, this, _1, _2),
		slot_num, slot_it->get_transfer()->file_block_count())));

	//check if we need to download this file
	bool already_downloading = false;
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Download_Slot.begin(), it_end = Download_Slot.end();
		it_cur != it_end; ++it_cur)
	{
		if(it_cur->second->hash() == hash){
			already_downloading = true;
			break;
		}
	}
	if(!already_downloading){
		for(share::slot_iterator it_cur = share::singleton().begin_slot(),
			it_end = share::singleton().end_slot(); it_cur != it_end; ++it_cur)
		{
			if(hash == it_cur->hash() && !it_cur->complete()){
				Hash_Pending.insert(hash);
				break;
			}
		}
	}

	return true;
}

bool slot_manager::recv_request_slot_failed()
{
	LOG << "stub: handle request slot failure";
	--open_slots;
	return true;
}

bool slot_manager::recv_slot(const unsigned char slot_num,
	const boost::uint64_t file_size, const std::string & root_hash,
	bit_field & tree_BF, bit_field & file_BF, const std::string hash)
{
	LOG << convert::abbr(hash);
	share::slot_iterator slot_it = share::singleton().find_slot(hash);
	if(slot_it == share::singleton().end_slot()){
		LOG << "failed " << convert::abbr(hash);
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::close_slot(slot_num)));
		return true;
	}

	//file size and root hash might not be known, set them
	if(!slot_it->set_unknown(Exchange.connection_ID, file_size, root_hash)){
		LOG << "error setting file size and root hash";
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::close_slot(slot_num)));
		return true;
	}

	//read bit field(s) (if any exist)
	if(!tree_BF.empty() && !file_BF.empty()){
		//unexpect previous have_hash_tree_block message with this slot (if exists)
		Exchange.expect_anytime_erase(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::have_hash_tree_block(slot_num, 0, 1)));

		//expect have_hash_tree_block message
		Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(new
			message_tcp::recv::have_hash_tree_block(boost::bind(&slot_manager::recv_have_hash_tree_block,
			this, _1, _2), slot_num, tree_BF.size())));

		//unexpect previous have_file_block message with this slot (if exists)
		Exchange.expect_anytime_erase(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::have_file_block(slot_num, 0, 1)));

		//expect have_hash_tree_block message
		Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(new
			message_tcp::recv::have_file_block(boost::bind(&slot_manager::recv_have_file_block,
			this, _1, _2), slot_num, file_BF.size())));
	}else if(!file_BF.empty()){
		//unexpect previous have_file_block message with this slot (if exists)
		Exchange.expect_anytime_erase(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::have_file_block(slot_num, 0, 1)));

		//expect have_hash_tree_block message
		Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(new
			message_tcp::recv::have_file_block(boost::bind(&slot_manager::recv_have_file_block,
			this, _1, _2), slot_num, file_BF.size())));
	}else if(!tree_BF.empty()){
		//unexpect previous have_hash_tree_block message with this slot (if exists)
		Exchange.expect_anytime_erase(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::have_hash_tree_block(slot_num, 0, 1)));

		//expect have_hash_tree_block message
		Exchange.expect_anytime(boost::shared_ptr<message_tcp::recv::base>(new
			message_tcp::recv::have_hash_tree_block(boost::bind(
			&slot_manager::recv_have_hash_tree_block, this, _1, _2), slot_num, tree_BF.size())));
	}
	slot_it->get_transfer()->download_reg(Exchange.connection_ID,
		*remote_listen, tree_BF, file_BF);

	//add outgoing slot
	std::pair<std::map<unsigned char, boost::shared_ptr<slot> >::iterator, bool>
		ret = Download_Slot.insert(std::make_pair(slot_num, slot_it.get()));
	if(ret.second == false){
		//host sent duplicate slot
		LOG << "violated protocol";
		return false;
	}
	return true;
}

void slot_manager::remove(const std::string & hash)
{
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Upload_Slot.begin(); it_cur != Upload_Slot.end();)
	{
		if(it_cur->second->hash() == hash){
			/*
			The next request made for this slot will result in an error and the
			other side will close it's slot.
			*/
			if(it_cur->second->get_transfer()){
				it_cur->second->get_transfer()->upload_unreg(Exchange.connection_ID);
			}
			Upload_Slot.erase(it_cur++);
		}else{
			++it_cur;
		}
	}
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Download_Slot.begin(); it_cur != Download_Slot.end();)
	{
		if(it_cur->second->hash() == hash){
			if(it_cur->second->get_transfer()){
				it_cur->second->get_transfer()->download_unreg(Exchange.connection_ID);
			}
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(
				new message_tcp::send::close_slot(it_cur->first)));
			--open_slots;
			Download_Slot.erase(it_cur++);
		}else{
			++it_cur;
		}
	}
	Hash_Pending.erase(hash);
}

void slot_manager::send_block_requests()
{
	if(Download_Slot.empty()){
		return;
	}
	std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Download_Slot.upper_bound(latest_slot);
	if(it_cur == Download_Slot.end()){
		it_cur = Download_Slot.begin();
	}

	/*
	These are used to break out of the loop if we loop through all slots and none
	need to make a request.
	*/
	unsigned char start_slot = it_cur->first;
	bool serviced_one = false;

	while(outgoing_pipeline_size < protocol_tcp::max_block_pipeline){
		if(it_cur->second->get_transfer()){
			if(boost::optional<transfer::next_request> NR
				= it_cur->second->get_transfer()->next_request_tree(Exchange.connection_ID))
			{
				++outgoing_pipeline_size;
				serviced_one = true;
				latest_slot = it_cur->first;
				Exchange.send(boost::shared_ptr<message_tcp::send::base>(
					new message_tcp::send::request_hash_tree_block(it_cur->first,
					NR->block_num, it_cur->second->get_transfer()->tree_block_count())));
				boost::shared_ptr<message_tcp::recv::composite> M_composite(
					new message_tcp::recv::composite());
				M_composite->add(boost::shared_ptr<message_tcp::recv::block>(
					new message_tcp::recv::block(boost::bind(
					&slot_manager::recv_hash_tree_block, this, _1, it_cur->first, NR->block_num),
					NR->block_size,
					it_cur->second->get_transfer()->download_speed_composite(Exchange.connection_ID))));
				M_composite->add(boost::shared_ptr<message_tcp::recv::error>(
					new message_tcp::recv::error(boost::bind(
					&slot_manager::recv_request_block_failed, this, it_cur->first))));
				Exchange.expect_response(M_composite);
			}else if(boost::optional<transfer::next_request> NR
				= it_cur->second->get_transfer()->next_request_file(Exchange.connection_ID))
			{
				++outgoing_pipeline_size;
				serviced_one = true;
				latest_slot = it_cur->first;
				Exchange.send(boost::shared_ptr<message_tcp::send::base>(
					new message_tcp::send::request_file_block(it_cur->first, NR->block_num,
					it_cur->second->get_transfer()->file_block_count())));
				boost::shared_ptr<message_tcp::recv::composite> M_composite(
					new message_tcp::recv::composite());
				M_composite->add(boost::shared_ptr<message_tcp::recv::block>(
					new message_tcp::recv::block(boost::bind(
					&slot_manager::recv_file_block, this, _1, it_cur->first, NR->block_num),
					NR->block_size,
					it_cur->second->get_transfer()->download_speed_composite(Exchange.connection_ID))));
				M_composite->add(boost::shared_ptr<message_tcp::recv::error>(
					new message_tcp::recv::error(
					boost::bind(&slot_manager::recv_request_block_failed, this, it_cur->first))));
				Exchange.expect_response(M_composite);
			}
		}
		++it_cur;
		if(it_cur == Download_Slot.end()){
			it_cur = Download_Slot.begin();
		}
		if(it_cur->first == start_slot && !serviced_one){
			//looped through all sockets and none needed to be serviced
			break;
		}else if(it_cur->first == start_slot && serviced_one){
			//reset flag for next loop through slots
			serviced_one = false;
		}
	}
}

void slot_manager::send_have()
{
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Upload_Slot.begin(), it_end = Upload_Slot.end();
		it_cur != it_end; ++it_cur)
	{
		if(!it_cur->second->get_transfer()){
			continue;
		}
		while(boost::optional<boost::uint64_t> block_num
			= it_cur->second->get_transfer()->next_have_tree(Exchange.connection_ID))
		{
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(
				new message_tcp::send::have_hash_tree_block(it_cur->first, *block_num,
				it_cur->second->get_transfer()->tree_block_count())));
		}
		while(boost::optional<boost::uint64_t> block_num
			= it_cur->second->get_transfer()->next_have_file(Exchange.connection_ID))
		{
			Exchange.send(boost::shared_ptr<message_tcp::send::base>(
				new message_tcp::send::have_file_block(it_cur->first, *block_num,
				it_cur->second->get_transfer()->file_block_count())));
		}
	}
}

void slot_manager::send_peer()
{
	for(std::map<unsigned char, boost::shared_ptr<slot> >::iterator
		it_cur = Upload_Slot.begin(), it_end = Upload_Slot.end();
		it_cur != it_end; ++it_cur)
	{
		if(it_cur->second->get_transfer()){
			while(boost::optional<net::endpoint>
				ep = it_cur->second->get_transfer()->next_peer(Exchange.connection_ID))
			{
				Exchange.send(boost::shared_ptr<message_tcp::send::base>(
					new message_tcp::send::peer(it_cur->first, *ep)));
			}
		}
	}
}

void slot_manager::send_slot_requests()
{
	while(!Hash_Pending.empty() && open_slots < 256){
		share::slot_iterator slot_it = share::singleton().find_slot(*Hash_Pending.begin());
		Hash_Opened.insert(*Hash_Pending.begin());
		Hash_Pending.erase(Hash_Pending.begin());
		if(slot_it == share::singleton().end_slot()){
			continue;
		}
		++open_slots;
		Exchange.send(boost::shared_ptr<message_tcp::send::base>(
			new message_tcp::send::request_slot(slot_it->hash())));
		boost::shared_ptr<message_tcp::recv::composite> M_composite(
			new message_tcp::recv::composite());
		M_composite->add(boost::shared_ptr<message_tcp::recv::base>(
			new message_tcp::recv::slot(boost::bind(&slot_manager::recv_slot, this,
			_1, _2, _3, _4, _5, slot_it->hash()), slot_it->hash())));
		M_composite->add(boost::shared_ptr<message_tcp::recv::base>(
			new message_tcp::recv::error(boost::bind(
			&slot_manager::recv_request_slot_failed, this))));
		Exchange.expect_response(M_composite);
	}
}

void slot_manager::sent_block()
{
	--incoming_pipeline_size;
	assert(incoming_pipeline_size < protocol_tcp::max_block_pipeline);
}

void slot_manager::set_remote_listen(const net::endpoint & ep)
{
	remote_listen.reset(new net::endpoint(ep));
}

void slot_manager::tick()
{
	close_complete();
	send_block_requests();
	send_have();
	send_peer();
	send_slot_requests();
}
