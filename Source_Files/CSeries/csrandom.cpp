
#include "csrandom.hpp"


#define DEFAULT_RANDOM_SEED ((uint16_t)0xfded)


static uint16_t random_seed       = 0x1;
static uint16_t local_random_seed = 0x1;


void set_random_seed(uint16_t seed)
{
    random_seed = seed ? seed : DEFAULT_RANDOM_SEED;
}


uint16_t get_random_seed()
{
    return random_seed;
}


uint16_t global_random()
{
    uint16_t seed = random_seed;
    
    if (seed & 1)
    {
        seed = (seed >> 1) ^ 0xb400;
    }
    else
    {
        seed >>= 1;
    }

    return (random_seed= seed);
}


uint16_t local_random()
{
    uint16_t seed = local_random_seed;
    
    if (seed & 1)
    {
        seed = (seed >> 1) ^ 0xb400;
    }
    else
    {
        seed >>= 1;
    }

    return (local_random_seed = seed);
}

