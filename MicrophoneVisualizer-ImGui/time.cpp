
#include "time.h"

double Time_Get(void)
{
    uint64_t one_hundred_ns_intervals;
    uint64_t fractional_part;
    uint64_t whole_part;

    FILETIME ftime;

    GetSystemTimeAsFileTime(&ftime);
    
    one_hundred_ns_intervals = ftime.dwHighDateTime;
    one_hundred_ns_intervals <<= 32;
    one_hundred_ns_intervals += ftime.dwLowDateTime;
    fractional_part = one_hundred_ns_intervals % 10000000LLU;
    whole_part = one_hundred_ns_intervals / 10000000LLU;
    return ((double)whole_part + ((double)fractional_part / 10000000.0));

}
