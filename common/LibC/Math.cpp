#include <System/Types.h>
#include <cmath>

double trunc(double x)
{
    return x >= 0 || double(s64(x)) == x ? double(s64(x)) : double(s64(x)) - 1;
}

double fmod(double x, double y)
{
    return x - trunc(x / y) * y;
}
