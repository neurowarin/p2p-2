//include
#include <bit_field.hpp>
#include <logger.hpp>

//standard
#include <cmath>

int fail(0);

void assignment()
{
	//starting with every bit off, turn every even bit on
	int test_size = 16;
	bit_field BF(test_size);
	for(int x=0; x<test_size; ++x){
		if(x % 2 == 0){
			BF[x] = true;
		}
	}

	//make sure every even bit on
	for(int x=0; x<test_size; ++x){
		if(x % 2 == 0){
			if(BF[x] != true){
				LOG; ++fail;
			}
		}
	}

	//starting with every bit on, turn every even bit off
	BF.set();
	for(int x=0; x<test_size; ++x){
		if(x % 2 == 0){
			BF[x] = false;
		}
	}

	//make sure every even bit off
	for(int x=0; x<test_size; ++x){
		if(x % 2 == 0){
			if(BF[x] != false){
				LOG; ++fail;
			}
		}
	}
}

void named_functions()
{
	bit_field BF(1);

	//size function
	if(BF.size() != 1){
		LOG; ++fail;
	}

	//reset function
	BF[0] = 1;
	if(BF[0] != 1){
		LOG; ++fail;
	}
	BF.reset();
	if(BF[0] == 1){
		LOG; ++fail;
	}
}

void operators()
{
	bit_field BF_0(2);
	bit_field BF_1(2);

	//==
	if(!(BF_0 == BF_1)){
		LOG; ++fail;
	}

	//!=
	if(BF_0 != BF_1){
		LOG; ++fail;
	}

	//&=
	BF_0 &= BF_1;
	if(BF_0[0] != 0){
		LOG; ++fail;
	}

	//^=
	BF_0 ^= BF_1;
	if(BF_0[0] != 0){
		LOG; ++fail;
	}

	//|=
	BF_0 |= BF_1;
	if(BF_0[0] != 0){
		LOG; ++fail;
	}

	//~
	~BF_0;
	if(BF_0[0] != 1){
		LOG; ++fail;
	}

	//=
	BF_0 = BF_1;
	if(BF_0[0] != 0){
		LOG; ++fail;
	}

	//bitgroup_vector::proxy assignment test
	BF_0[0] = BF_0[1] = 1;
	if(BF_0[0] != 1 || BF_0[1] != 1){
		LOG; ++fail;
	}
}

int main()
{
	assignment();
	named_functions();
	operators();
	return fail;
}
