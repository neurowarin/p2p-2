#ifndef H_MPINT
#define H_MPINT

//include
#include <CMWC4096.hpp>
#include <random.hpp>
#include <tommath/tommath.h>

//standard
#include <cstdlib>
#include <iostream>

class ltmpp_error : public std::exception
{
public:
	ltmpp_error(int c):type(c){}
	const char * what(){
		switch (type) {
		case MP_MEM: return "out of memory";
		case MP_VAL: return "invalid value";
		default    : return "unknown error";
		}
   }
private:
	int type;
};

class mpint
{
public:
	mpint();
	mpint(const char * str, int radix = 10);
	mpint(const unsigned char * bin, int len);
	mpint(const mpint & b);
	~mpint();

	/* Get Representation Functions
	to_bin:
		Returns array to binary (big-endian) of mpint. Call to_bin_size for the
		length.
	to_bin_size:
		Length of array returned by to_bin.
	to_str:
		Returns mpint in specified base (2 to 64).
	*/
   unsigned char * to_bin();
   int to_bin_size();
	char * to_str(int radix);

	void operator = (const mpint & b);
	mpint operator + (const mpint & b);
   mpint operator - (const mpint & b);
	mpint operator * (const mpint & b);
	mpint operator / (const mpint & b);
	mpint operator % (const mpint & b);
   mpint operator ^ (const mpint & b);
	mpint operator | (const mpint & b);
	mpint operator & (const mpint & b);
   mpint operator - ();
	mpint operator << (int n);
   mpint operator >> (int n);
	mpint & operator += (const mpint & b);
	mpint & operator -= (const mpint & b);
   mpint & operator *= (const mpint & b);
	mpint & operator /= (const mpint & b);
	mpint & operator %= (const mpint & b);
	mpint & operator ^= (const mpint & b);
	mpint & operator &= (const mpint & b);
	mpint & operator |= (const mpint & b);
	mpint & operator <<= (int n);
	mpint & operator >>= (int n);
   mpint & operator ++ ();
	mpint & operator -- ();
	mpint & operator ++ (int);
	mpint & operator -- (int);
	mpint & operator += (const mp_digit b);
	mpint & operator -= (const mp_digit b);
	mpint & operator *= (const mp_digit b);
	mpint & operator /= (const mp_digit b);
	mpint & operator %= (const mp_digit b);
   bool operator == (const mpint & b);
   bool operator != (const mpint & b);
   bool operator > (const mpint & b);
   bool operator < (const mpint & b);
   bool operator >= (const mpint & b);
   bool operator <= (const mpint & b);
	bool operator == (const mp_digit b);
	bool operator != (const mp_digit b);
	bool operator > (const mp_digit b);
	bool operator >= (const mp_digit b);
	bool operator < (const mp_digit b);
	bool operator <= (const mp_digit b);

	/* Math Functions
	gcd:
		Greatest common denominator.
	lcm:
		Least common multiple.
	invmod:
		Inverse modulo.
	exptmod:
		(this number)**y mod(p)
	*/
   mpint gcd(const mpint & b);
	mpint lcm(const mpint & b);
	mpint invmod(const mpint & b);
	mpint exptmod(const mpint & y, const mpint & p);

	//returns true if mpint is prime
	bool is_prime();

	//returns a random mpint of the specified size (bytes)
	static mpint random(const int bytes, int (*RNG)(unsigned char *, int, void *))
	{
		unsigned char * buff = static_cast<unsigned char *>(std::malloc(bytes));
		RNG(buff, bytes, NULL);
		mpint temp(buff, bytes);
		std::free(buff);
		return temp;
	}

	/*
	Returns a random prime of the specified bytes. This version uses the
	operating system's built in entropy random source. This will produce primes
	that are definetly good for cryptography. However, it is slow.
	*/
	static mpint random_prime(const int bytes)
	{
		mpint temp;
		mp_prime_random_ex(
			&temp.data,
			mp_prime_rabin_miller_trials(bytes * 8),
			bytes * 8,         //size (bits) of prime to generate
			0,                 //optional flags
			&portable_urandom, //random byte source
			NULL               //optional void * passed to PRNG
		);
		return temp;
	}

	/*
	Returns a random prime of the specified bytes. This version uses a specified
	CMWC4096 PRNG which is VERY fast but is not a cryptographically secure PRNG.
	*/
	static mpint random_prime_fast(const int bytes, CMWC4096 & PRNG)
	{
		mpint temp;
		mp_prime_random_ex(
			&temp.data,
			mp_prime_rabin_miller_trials(bytes * 8),
			bytes * 8,         //size (bits) of prime to generate
			0,                 //optional flags
			&CMWC4096_wrapper, //random byte source
			&PRNG              //optional void * passed to PRNG
		);
		return temp;
	}

	//prints mpint in base 10
	friend std::ostream & operator << (std::ostream & lval, mpint & rval)
	{
		return lval << rval.to_str(10);
	}

private:
	static int CMWC4096_wrapper(unsigned char * buff, int size, void * data)
	{
		CMWC4096 * PRNG = reinterpret_cast<CMWC4096 *>(data);
		PRNG->bytes(buff, size);
		return size;
	}

	mp_int data;
	char * rstr;
};
#endif
