#include "tommath.h"
#ifdef BN_MP_MONTGOMERY_SETUP_C

/* setups the montgomery reduction stuff */
int mp_montgomery_setup(mp_int * n, mp_digit * rho)
{
	mp_digit x, b;

	/* fast inversion mod 2**k
	 *
	 * Based on the fact that
	 *
	 * XA = 1 (mod 2**n)  =>  (X(2-XA)) A = 1 (mod 2**2n)
	 *                    =>  2*X*A - X*X*A*A = 1
	 *                    =>  2*(1) - (1)     = 1
	 */
	b = n->dp[0];

	if((b & 1) == 0){
		return MP_VAL;
	}

	x = (((b + 2) & 4) << 1) + b; /* here x*a==1 mod 2**4 */
	x *= 2 - b * x;               /* here x*a==1 mod 2**8 */
#if !defined(MP_8BIT)
	x *= 2 - b * x;               /* here x*a==1 mod 2**16 */
#endif
#if defined(MP_64BIT) || !(defined(MP_8BIT) || defined(MP_16BIT))
	x *= 2 - b * x;               /* here x*a==1 mod 2**32 */
#endif
#ifdef MP_64BIT
	x *= 2 - b * x;               /* here x*a==1 mod 2**64 */
#endif

	/* rho = -1/m mod b */
	*rho = (((mp_word)1 << ((mp_word)DIGIT_BIT)) - x) & MP_MASK;

	return MP_OKAY;
}
#endif
