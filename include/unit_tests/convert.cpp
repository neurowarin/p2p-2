//include
#include <convert.hpp>
#include <logger.hpp>
#include <unit_test.hpp>

//standard
#include <string>

int fail(0);

int main()
{
	unit_test::timeout();

	//encode/decode
	if(convert::bin_to_int<int>(convert::int_to_bin<int>(10)) != 10){
		LOG; ++fail;
	}

	//encode_VLI/decode_VLI
	if(convert::VLI_size(1) != 1){
		LOG; ++fail;
	}
	if(convert::VLI_size(255) != 1){
		LOG; ++fail;
	}
	if(convert::VLI_size(256) != 2){
		LOG; ++fail;
	}
	if(convert::bin_VLI_to_int(convert::int_to_bin_VLI(10, 11)) != 10){
		LOG; ++fail;
	}

	//hex_to_bin -> bin_to_hex
	std::string hex = "0123456789ABCDEF";
	if(convert::bin_to_hex(convert::hex_to_bin(hex)) != hex){
		LOG; ++fail;
	}

	//size comparisons
	std::string size_0 = convert::bytes_to_SI(1024);
	std::string size_1 = convert::bytes_to_SI(512);
	if(convert::SI_cmp(size_0, size_1) != 1){
		LOG; ++fail;
	}
	size_0 = convert::bytes_to_SI(512);
	size_1 = convert::bytes_to_SI(1024);
	if(convert::SI_cmp(size_0, size_1) != -1){
		LOG; ++fail;
	}
	return fail;
}
