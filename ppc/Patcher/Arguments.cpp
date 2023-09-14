#include "Arguments.hpp"
#include <Debug/Log.hpp>
#include <System/Util.h>
#include <array>
#include <cstring>

namespace Patcher
{

#define COMMAND_LINE_OPTIONS                                                   \
    X(OPT_RIIVO_XML, "--riivo-xml", true,                                      \
      "Defines a path to a Riivolution XML. Passing a directory will search "  \
      "the entire directory. By default, all XMLs discovered on the disk "     \
      "will be read. If this option is used, it will be restricted to any "    \
      "paths defined by the user.")                                            \
    X(OPT_PATCH_ID, "--patch-id", true,                                        \
      "Patches the game using the specified Riivolution Patch ID.")

enum class ArgOption {
    OPT_UNKNOWN = -1,

#define X(_ENUM, _STR, _HAS_VALUE, _DESCRIPTION) _ENUM,
    COMMAND_LINE_OPTIONS
#undef X
};

static ArgOption GetOptionByName(const char* name, const u32 nameLen)
{
#define X(_ENUM, _STR, _HAS_VALUE, _DESCRIPTION)                               \
    if (std::strncmp(name, _STR, nameLen) == 0 && _STR[nameLen] == '\0') {     \
        return ArgOption::_ENUM;                                               \
    }

    COMMAND_LINE_OPTIONS
#undef X

    return ArgOption::OPT_UNKNOWN;
}

static bool ArgOption_HasValue(ArgOption option)
{
    static const bool s_optionHasValue[] = {
#define X(_ENUM, _STR, _HAS_VALUE, _DESCRIPTION) _HAS_VALUE,
        COMMAND_LINE_OPTIONS
#undef X
    };

    u32 index = static_cast<u32>(option);
    ASSERT(index < std::size(s_optionHasValue));

    return s_optionHasValue[index];
}

/**
 * Create a patcher arguments struct from command line arguments passed
 * using wiiload or through launching the title.
 */
Arguments Arguments::ParseCommandLine(const u32 argc, const char* const* argv)
{
    Arguments arguments = {};

    // The first argument is usually reserved for the program name, but when
    // called through wiiload _with arguments_ it actually skips it for some
    // reason. We can just have user put "launch" or something of the like.
    u32 index = 1;

    for (; index < argc; index++) {
        const char* arg = argv[index];

        if (arg == nullptr) {
            continue;
        }

        if (std::strncmp(arg, "--", 2) != 0) {
            PRINT(
                Patcher, WARN,
                "Skipping argument '%s' supplied without command marker '--'",
                arg
            );
            continue;
        }

        ArgOption option = ArgOption::OPT_UNKNOWN;
        const char* optionName = arg;
        const char* optionNameEnd = std::strchr(optionName, '=');
        u32 optionNameLen = 0;

        const char* value = nullptr;

        if (optionNameEnd == nullptr) {
            optionNameLen = std::strlen(optionName);
            option = GetOptionByName(optionName, optionNameLen);

            if (option != ArgOption::OPT_UNKNOWN &&
                ArgOption_HasValue(option) && ++index < argc) {
                value = argv[index];
            }
        } else {
            optionNameLen = optionNameEnd - optionName;
            option = GetOptionByName(optionName, optionNameLen);

            if (option != ArgOption::OPT_UNKNOWN &&
                ArgOption_HasValue(option)) {
                value = optionNameEnd + 1;

                if (*value == '\0') {
                    value = nullptr;
                }
            }
        }

        if (option != ArgOption::OPT_UNKNOWN && ArgOption_HasValue(option) &&
            value == nullptr) {
            PRINT(
                Patcher, WARN,
                "Skipping argument '%.*s' supplied without value",
                optionNameLen, optionName
            );
            continue;
        }

        switch (option) {
        default:
            PRINT(
                Patcher, WARN, "Skipping unrecognized argument '%.*s'",
                optionNameLen, optionName
            );
            break;

        case ArgOption::OPT_RIIVO_XML:
            PRINT(Patcher, INFO, "--riivo-xml: %s", value);
            break;

        case ArgOption::OPT_PATCH_ID:
            PRINT(Patcher, INFO, "--patch-id: %s", value);
            break;
        }
    }

    return arguments;
}

} // namespace Patcher
