#include <IOS/System.hpp>
#include <cstdlib>
#include <unistd.h>

void usleep(unsigned int usec)
{
    System::SleepUsec(usec);
}

void abort()
{
    System::Abort();
    __builtin_unreachable();
}
