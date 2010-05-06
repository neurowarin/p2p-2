#include "hash_tree.hpp"

hash_tree::hash_tree(
	const file_info & FI,
	db::pool::proxy DB
):
	hash(FI.hash),
	path(FI.path),
	file_size(FI.file_size),
	row(file_size_to_row(file_size)),
	tree_block_count(row_to_tree_block_count(row)),
	file_hash_offset(row_to_file_hash_offset(row)),
	tree_size(file_size_to_tree_size(file_size)),
	file_block_count(
		file_size % protocol_tcp::file_block_size == 0 ?
			file_size / protocol_tcp::file_block_size :
			file_size / protocol_tcp::file_block_size + 1
	),
	last_file_block_size(
		file_size % protocol_tcp::file_block_size == 0 ?
			protocol_tcp::file_block_size :
			file_size % protocol_tcp::file_block_size
	)
{
	if(!hash.empty()){
		boost::shared_ptr<db::table::hash::info>
			info = db::table::hash::find(hash, DB);
		if(info){
			//opened existing hash tree
			boost::uint64_t size;
			if(!db::pool::get()->blob_size(info->blob, size)){
				throw std::runtime_error("error checking tree size");
			}
			//only open tree if the size is what we expect
			if(tree_size != size){
				/*
				It is possible someone might encounter two files of different sizes
				that have the same hash. We don't account for this because it's so
				rare. We just fail to create a slot for one of the files.
				*/
				throw std::runtime_error("incorrect tree size");
			}
			const_cast<db::blob &>(blob) = info->blob;
		}else{
			//allocate space to reconstruct hash tree
			if(db::table::hash::add(hash, tree_size, DB)){
				db::table::hash::set_state(hash,
					db::table::hash::downloading, DB);
				info = db::table::hash::find(hash, DB);
				if(info){
					const_cast<db::blob &>(blob) = info->blob;
				}else{
					throw std::runtime_error("failed hash tree open");
				}
			}else{
				throw std::runtime_error("failed hash tree allocate");
			}
		}
	}
}

bool hash_tree::block_info(const boost::uint64_t block,
	std::pair<boost::uint64_t, unsigned> & info)
{
	boost::uint64_t throw_away;
	return block_info(block, info, throw_away);
}

bool hash_tree::block_info(const boost::uint64_t block,
	std::pair<boost::uint64_t, unsigned> & info, boost::uint64_t & parent)
{
	/*
	For simplicity this function operates on RRNs until just before it is done
	when it converts to bytes.
	*/
	boost::uint64_t offset = 0;          //hash offset from beginning of file to start of row
	boost::uint64_t block_count = 0;     //total block count in all previous rows
	boost::uint64_t row_block_count = 0; //total block count in current row
	for(unsigned x=0; x<row.size(); ++x){
		if(row[x] % protocol_tcp::hash_block_size == 0){
			row_block_count = row[x] / protocol_tcp::hash_block_size;
		}else{
			row_block_count = row[x] / protocol_tcp::hash_block_size + 1;
		}

		//check if block we're looking for is in row
		if(block_count + row_block_count > block){
			info.first = offset + (block - block_count) * protocol_tcp::hash_block_size;

			//determine size of block
			boost::uint64_t delta = offset + row[x] - info.first;
			if(delta > protocol_tcp::hash_block_size){
				info.second = protocol_tcp::hash_block_size;
			}else{
				info.second = delta;
			}

			if(x != 0){
				//if not the root hash there is a parent
				parent = offset - row[x-1] + block - block_count;
			}

			//convert to bytes
			parent = parent * SHA1::bin_size;
			info.first = info.first * SHA1::bin_size;
			info.second = info.second * SHA1::bin_size;
			return true;
		}
		block_count += row_block_count;
		offset += row[x];
	}
	return false;
}

unsigned hash_tree::block_size(const boost::uint64_t block_num)
{
	std::pair<boost::uint64_t, unsigned> info;
	if(block_info(block_num, info)){
		return info.second;
	}else{
		LOG << "invalid block";
		exit(1);
	}
}

hash_tree::status hash_tree::check()
{
	net::buffer buf;
	buf.reserve(protocol_tcp::file_block_size);
	for(boost::uint32_t block_num=0; block_num<tree_block_count; ++block_num){
		buf.clear();
		status Status = read_block(block_num, buf);
		if(Status == bad){
			return io_error;
		}
		Status = check_tree_block(block_num, buf);
		if(Status == bad){
			return bad;
		}else if(Status == io_error){
			return io_error;
		}else if(block_num == tree_block_count - 1){
			//last block checked good
			db::table::hash::set_state(hash, db::table::hash::complete);
		}
	}
	return good;
}

hash_tree::status hash_tree::check_file_block(const boost::uint64_t file_block_num,
	const net::buffer & buf)
{
	char parent_buf[SHA1::bin_size];
	if(!db::pool::get()->blob_read(blob, parent_buf,
		SHA1::bin_size, file_hash_offset + file_block_num * SHA1::bin_size))
	{
		return io_error;
	}
	SHA1 SHA(reinterpret_cast<const char *>(buf.data()),
		buf.size());
	if(std::memcmp(parent_buf, SHA.bin(), SHA1::bin_size) == 0){
		return good;
	}else{
		return bad;
	}
}

hash_tree::status hash_tree::check_tree_block(const boost::uint64_t block_num,
	const net::buffer & buf)
{
	if(block_num == 0){
		//special requirements to check root_hash, see documentation for root_hash
		assert(buf.size() == SHA1::bin_size);
		char special[SHA1::bin_size + 8];
		std::memcpy(special, convert::int_to_bin(file_size).data(), 8);
		std::memcpy(special + 8, buf.data(), SHA1::bin_size);
		SHA1 SHA(special, SHA1::bin_size + 8);
		return SHA.hex() == hash ? good : bad;
	}else{
		std::pair<boost::uint64_t, unsigned> info;
		boost::uint64_t parent;
		if(!block_info(block_num, info, parent)){
			//invalid block
			LOG << "invalid block";
			exit(1);
		}
		assert(buf.size() == info.second);
		//create hash for children
		SHA1 SHA(reinterpret_cast<const char *>(buf.data()),
			buf.size());
		//verify parent hash is a hash of the children
		char parent_hash[SHA1::bin_size];
		if(!db::pool::get()->blob_read(blob, parent_hash, SHA1::bin_size, parent)){
			return io_error;
		}
		if(std::memcmp(parent_hash, SHA.bin(), SHA1::bin_size) == 0){
			return good;
		}else{
			return bad;
		}
	}
}

hash_tree::status hash_tree::create()
{
	//assert the hash tree has not yet been created
	assert(hash == "");

	//open file to generate hash tree for
	std::fstream file(path.c_str(), std::ios::in | std::ios::binary);
	if(!file.good()){
		LOG << "error opening " << path;
		return io_error;
	}

	/*
	Open temp file to write hash tree to. A temp file is used to avoid database
	contention.
	*/
	std::fstream temp(path::hash_tree_temp().c_str(), std::ios::in |
		std::ios::out | std::ios::trunc | std::ios::binary);
	if(!temp.good()){
		LOG << "error opening temp file";
		return io_error;
	}

	char buf[protocol_tcp::file_block_size];
	SHA1 SHA;

	//do file hashes
	std::time_t T(0);
	for(boost::uint64_t x=0; x<row.back(); ++x){
		//check once per second to make sure file not copying
		if(std::time(NULL) != T){
			try{
				if(file_size < boost::filesystem::file_size(path)){
					LOG << "copying " << path;
					return copying;
				}
				T = std::time(NULL);
			}catch(const std::exception & e){
				LOG << "error reading " << path;
				return io_error;
			}
		}

		if(boost::this_thread::interruption_requested()){
			return io_error;
		}
		file.read(buf, protocol_tcp::file_block_size);
		if(file.gcount() == 0){
			LOG << "error reading " << path;
			return io_error;
		}
		SHA.init();
		SHA.load(buf, file.gcount());
		SHA.end();
		temp.seekp(file_hash_offset + x * SHA1::bin_size, std::ios::beg);
		temp.write(SHA.bin(), SHA1::bin_size);
		if(!temp.good()){
			LOG << "error writing temp file";
			return io_error;
		}
	}

	//do all other tree hashes
	for(boost::uint64_t x=tree_block_count - 1; x>0; --x){
		std::pair<boost::uint64_t, unsigned> info;
		boost::uint64_t parent;
		if(!block_info(x, info, parent)){
			LOG << "invalid block";
			exit(1);
		}
		temp.seekg(info.first, std::ios::beg);
		temp.read(buf, info.second);
		if(temp.gcount() != info.second){
			LOG << "error reading temp file";
			return io_error;
		}
		SHA.init();
		SHA.load(buf, info.second);
		SHA.end();
		temp.seekp(parent, std::ios::beg);
		temp.write(SHA.bin(), SHA1::bin_size);
		if(!temp.good()){
			LOG << "error writing temp file";
			return io_error;
		}
	}

	//calculate hash
	std::memcpy(buf, convert::int_to_bin(file_size).data(), 8);
	std::memcpy(buf + 8, SHA.bin(), SHA1::bin_size);
	SHA.init();
	SHA.load(buf, SHA1::bin_size + 8);
	SHA.end();
	const_cast<std::string &>(hash) = SHA.hex();

	/*
	Copy hash tree in to database. No transaction is used because it this can
	tie up the database for too long when copying a large hash tree. The
	writes are already quite large so a transaction doesn't make much
	performance difference.
	*/

	//check if tree already exists
	if(db::table::hash::find(hash)){
		return good;
	}

	//tree doesn't exist, allocate space for it
	if(!db::table::hash::add(hash, tree_size)){
		LOG << "error adding hash tree";
		return io_error;
	}

	//copy tree to database using large buffer for performance
	boost::shared_ptr<db::table::hash::info>
		Info = db::table::hash::find(hash);

	//set blob to one we just created
	const_cast<db::blob &>(blob) = Info->blob;

	temp.seekg(0, std::ios::beg);
	boost::uint64_t offset = 0, bytes_remaining = tree_size, read_size;
	while(bytes_remaining){
		if(boost::this_thread::interruption_requested()){
			db::table::hash::remove(hash);
			return io_error;
		}
		if(bytes_remaining > protocol_tcp::file_block_size){
			read_size = protocol_tcp::file_block_size;
		}else{
			read_size = bytes_remaining;
		}
		temp.read(buf, read_size);
		if(temp.gcount() != read_size){
			LOG << "error reading temp file";
			db::table::hash::remove(hash);
			return io_error;
		}else{
			if(!db::pool::get()->blob_write(blob, buf, read_size, offset)){
				LOG << "error writing blob";
				db::table::hash::remove(hash);
				return io_error;
			}
			offset += read_size;
			bytes_remaining -= read_size;
		}
	}
	return good;
}

std::pair<std::pair<boost::uint64_t, boost::uint64_t>, bool>
	hash_tree::file_block_children(const boost::uint64_t block)
{
	std::pair<boost::uint64_t, unsigned> info;
	if(block_info(block, info)){
		std::pair<std::pair<boost::uint64_t, boost::uint64_t>, bool> pair;
		if(info.first >= file_hash_offset){
			pair.first.first = (info.first - file_hash_offset) / SHA1::bin_size;
			pair.first.second = ((info.first + info.second) - file_hash_offset)
				/ SHA1::bin_size;
			pair.second = true;
			return pair;
		}else{
			pair.second = false;
			return pair;
		}
	}else{
		LOG << "invalid block";
		exit(1);
	}
}

boost::uint64_t hash_tree::file_hash_to_tree_hash(boost::uint64_t row_hash,
	std::deque<boost::uint64_t> & row)
{
	boost::uint64_t start_hash = row_hash;
	row.push_front(row_hash);
	if(row_hash == 1){
		return 1;
	}else if(row_hash % protocol_tcp::hash_block_size == 0){
		row_hash = start_hash / protocol_tcp::hash_block_size;
	}else{
		row_hash = start_hash / protocol_tcp::hash_block_size + 1;
	}
	return start_hash + file_hash_to_tree_hash(row_hash, row);
}

boost::uint64_t hash_tree::file_size_to_file_hash(const boost::uint64_t file_size)
{
	boost::uint64_t hash_count = file_size / protocol_tcp::file_block_size;
	if(file_size % protocol_tcp::file_block_size != 0){
		//add one for partial last block
		++hash_count;
	}
	return hash_count;
}

std::deque<boost::uint64_t> hash_tree::file_size_to_row(const boost::uint64_t file_size)
{
	std::deque<boost::uint64_t> row;
	file_hash_to_tree_hash(file_size_to_file_hash(file_size), row);
	return row;
}

boost::uint64_t hash_tree::calc_tree_block_count(boost::uint64_t file_size)
{
	std::deque<boost::uint64_t> row = file_size_to_row(file_size);
	return row_to_tree_block_count(row);
}

boost::uint64_t hash_tree::file_size_to_tree_size(const boost::uint64_t file_size)
{
	if(file_size == 0){
		return 0;
	}else{
		std::deque<boost::uint64_t> throw_away;
		return SHA1::bin_size * file_hash_to_tree_hash(file_size_to_file_hash(file_size), throw_away);
	}
}

hash_tree::status hash_tree::read_block(const boost::uint64_t block_num,
	net::buffer & buf)
{
	std::pair<boost::uint64_t, unsigned> info;
	if(block_info(block_num, info)){
		buf.tail_reserve(info.second);
		if(!db::pool::get()->blob_read(blob,
			reinterpret_cast<char *>(buf.tail_start()), info.second, info.first))
		{
			buf.tail_resize(0);
			return io_error;
		}
		buf.tail_resize(info.second);
		return good;
	}else{
		LOG << "invalid block";
		exit(1);
	}
}

boost::uint64_t hash_tree::row_to_tree_block_count(
	const std::deque<boost::uint64_t> & row)
{
	boost::uint64_t block_count = 0;
	for(int x=0; x<row.size(); ++x){
		if(row[x] % protocol_tcp::hash_block_size == 0){
			block_count += row[x] / protocol_tcp::hash_block_size;
		}else{
			block_count += row[x] / protocol_tcp::hash_block_size + 1;
		}
	}
	return block_count;
}

boost::uint64_t hash_tree::row_to_file_hash_offset(
	const std::deque<boost::uint64_t> & row)
{
	boost::uint64_t file_hash_offset = 0;
	if(row.size() == 0){
		//no hash tree, root hash is file hash
		return file_hash_offset;
	}
	//add up size (bytes) of rows until reaching the last row
	for(int x=0; x<row.size()-1; ++x){
		file_hash_offset += row[x] * SHA1::bin_size;
	}
	return file_hash_offset;
}

std::pair<std::pair<boost::uint64_t, boost::uint64_t>, bool>
	hash_tree::tree_block_children(const boost::uint64_t block)
{
	boost::uint64_t offset = 0;          //hash offset from beginning of file to start of row
	boost::uint64_t block_count = 0;     //total block count in all previous rows
	boost::uint64_t row_block_count = 0; //total block count in current row
	for(unsigned x=0; x<row.size(); ++x){
		if(row[x] % protocol_tcp::hash_block_size == 0){
			row_block_count = row[x] / protocol_tcp::hash_block_size;
		}else{
			row_block_count = row[x] / protocol_tcp::hash_block_size + 1;
		}
		//check if block we're looking for is in row
		if(block_count + row_block_count > block){
			//offset to start of block (RRN, hash offset)
			boost::uint64_t block_offset = offset + (block - block_count)
				* protocol_tcp::hash_block_size;

			if(block_offset * SHA1::bin_size >= file_hash_offset){
				//leaf, no hash block children
				std::pair<std::pair<boost::uint64_t, boost::uint64_t>, bool> pair;
				pair.second = false;
				return pair;
			}

			//determine size of block
			boost::uint64_t delta = offset + row[x] - block_offset;
			unsigned block_size;
			if(delta > protocol_tcp::hash_block_size){
				block_size = protocol_tcp::hash_block_size;
			}else{
				block_size = delta;
			}

			std::pair<std::pair<boost::uint64_t, boost::uint64_t>, bool> pair;
			pair.second = true;
			pair.first.first =
				(block_offset - offset) +        //parents in current row before block
				(block_count + row_block_count); //block number of start of next row
			pair.first.second = pair.first.first + block_size;
			return pair;
		}
		block_count += row_block_count;
		offset += row[x];
	}
	LOG << "invalid block";
	exit(1);
}

hash_tree::status hash_tree::write_block(const boost::uint64_t block_num,
	const net::buffer & buf)
{
	std::pair<boost::uint64_t, unsigned> info;
	boost::uint64_t parent;
	if(block_info(block_num, info, parent)){
		assert(info.second == buf.size());
		if(block_num != 0){
			char parent_buf[SHA1::bin_size];
			if(!db::pool::get()->blob_read(blob, parent_buf, SHA1::bin_size, parent)){
				return io_error;
			}
			SHA1 SHA(reinterpret_cast<const char *>(buf.data()),
				info.second);
			if(std::memcmp(parent_buf, SHA.bin(), SHA1::bin_size) != 0){
				return bad;
			}
		}
		if(!db::pool::get()->blob_write(blob,
			reinterpret_cast<const char *>(buf.data()), buf.size(), info.first))
		{
			return io_error;
		}
		return good;
	}else{
		LOG << "invalid block";
		exit(1);
	}
}
