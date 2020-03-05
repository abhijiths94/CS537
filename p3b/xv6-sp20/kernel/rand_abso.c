#include "rand.h"

int s = 1;

int xorshift32()
{
	int x = s;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	return s = x;
}

int xv6_rand (void){
    int a = (xorshift32() & XV6_RAND_MAX);
    return a;
}

void xv6_srand (unsigned int seed){
    s =  seed;
}
