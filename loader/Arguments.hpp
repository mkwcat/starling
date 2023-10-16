#pragma once

#include <Types.h>

class Arguments
{
public:
    /**
     * Create a patcher arguments struct from command line arguments passed
     * using wiiload or through launching the title.
     */
    static Arguments ParseCommandLine(const u32 argc, const char* const* argv);

    /**
     * Check if the arguments are complete enough to start the game.
     */
    bool IsStartReady() const;

private:
};
