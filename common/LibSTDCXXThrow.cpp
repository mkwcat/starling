#include <stdexcept>
#include <Log.hpp>

void std::__throw_length_error(const char* str)
{
    PRINT(LibCPP, ERROR, "std::__throw_length_error: %s", str);
    abort();
}

void std::__throw_bad_array_new_length()
{
    PRINT(LibCPP, ERROR, "std::__throw_bad_array_new_length");
    abort();
}

void std::__throw_bad_alloc()
{
    PRINT(LibCPP, ERROR, "std::__throw_bad_alloc");
    abort();
}

void std::__throw_bad_function_call() {
    PRINT(LibCPP, ERROR, "std::__throw_bad_function_call");
    abort();
}