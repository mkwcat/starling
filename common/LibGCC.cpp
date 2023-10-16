extern "C" {
void atexit()
{
}

bool __cxa_guard_acquire(bool* guard)
{
    return !(*guard);
}

void __cxa_guard_release(bool* guard)
{
    *guard = 1;
}
}
