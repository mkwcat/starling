#include "Arguments.hpp"
#include "PatchManager.hpp"
#include <Log.hpp>
#include <Util.h>
#include <array>
#include <cstring>

Arguments::ArgOption
Arguments::GetOptionByName(const char* name, const u32 nameLen)
{
#define X(_ENUM, _STR, _HAS_VALUE, _DESCRIPTION)                               \
    if (std::strncmp(name, _STR, nameLen) == 0 && _STR[nameLen] == '\0') {     \
        return ArgOption::_ENUM;                                               \
    }

    COMMAND_LINE_OPTIONS
#undef X

    return ArgOption::OPT_UNKNOWN;
}

bool Arguments::ArgOption_HasValue(ArgOption option)
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

bool Arguments::Handle(
    bool printErrors, ArgOption handleOption,
    bool (*callback)(ArgOption option, const char* value, void* userData),
    void* userData
) const
{
    // The first argument is usually reserved for the program name, but when
    // called through wiiload _with arguments_ it actually skips it for some
    // reason. We can just have user put "launch" or something of the like.
    u32 index = 1;

    for (; index < m_argc; index++) {
        const char* arg = m_argv[index];

        if (arg == nullptr) {
            continue;
        }

        if (std::strncmp(arg, "--", 2) != 0) {
            if (printErrors) {
                PRINT(
                    Patcher, WARN,
                    "Skipping argument '%s' supplied without command marker "
                    "'--'",
                    arg
                );
            }
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
                ArgOption_HasValue(option) && ++index < m_argc) {
                value = m_argv[index];
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
            if (printErrors) {
                PRINT(
                    Patcher, WARN,
                    "Skipping argument '%.*s' supplied without value",
                    optionNameLen, optionName
                );
            }
            continue;
        }

        // Check if the option is valid
        switch (option) {
        default:
            if (printErrors) {
                PRINT(
                    Patcher, WARN, "Skipping unrecognized argument '%.*s'",
                    optionNameLen, optionName
                );
            }
            break;

        case ArgOption::OPT_LAUNCH:
            PRINT(Patcher, INFO, "--launch: %s", value);
            break;

        case ArgOption::OPT_RIIVO_XML:
            PRINT(Patcher, INFO, "--riivo-xml: %s", value);
            break;

        case ArgOption::OPT_PATCH_ID:
            PRINT(Patcher, INFO, "--patch-id: %s", value);
            break;
        }

        if (callback == nullptr || option == ArgOption::OPT_UNKNOWN ||
            (handleOption != ArgOption::OPT_UNKNOWN && option != handleOption
            )) {
            continue;
        }

        if (!callback(option, value)) {
            if (printErrors) {
                PRINT(
                    Patcher, WARN, "Failed to handle argument '%.*s'",
                    optionNameLen, optionName
                );
            }
            return false;
        }
    }

    return true;
}

bool Arguments::HasOption(ArgOption option) const
{
    struct HasOptionData {
        ArgOption option;
        bool hasOption;
    } hasOptionData = {option, false};

    Handle(
        false, ArgOption::OPT_UNKNOWN,
        [](ArgOption option, const char* value, void* userData) -> bool {
            HasOptionData* hasOptionData =
                static_cast<HasOptionData*>(userData);

            if (option == hasOptionData->option) {
                hasOptionData->hasOption = true;
                return false;
            }

            return true;
        },
        &hasOptionData
    );

    return hasOptionData.hasOption;
}

bool Arguments::Validate() const
{
    if (m_argc == 0) {
        return true;
    }

    if (m_argv == nullptr) {
        return false;
    }

    return Handle(true, ArgOption::OPT_UNKNOWN);
}

bool Arguments::IsStartReady() const
{
    if (m_argc == 0 || m_argv == nullptr) {
        return false;
    }

    struct ReadyData {
        bool ready;
    } readyData = {false};

    return Handle(
        false, ArgOption::OPT_UNKNOWN,
        [](ArgOption option, const char* value, void* userData) -> bool {
            ReadyData* readyData = static_cast<ReadyData*>(userData);

            switch (option) {
            case ArgOption::OPT_LAUNCH:
                readyData->ready = true;
                break;

                // If patch ID is specified on its own, /dev/di is used for
                // launch by default
            case ArgOption::OPT_PATCH_ID:
                readyData->ready = true;
                break;

            default:
                break;
            }

            return true;
        },
        &readyData
    );
}

void Arguments::Launch()
{
    if (m_argc == 0 || m_argv == nullptr) {
        return false;
    }

    struct LaunchData {
        bool ready;
        bool hasRiivoXml;
        bool hasPatchId;
        const char* launchPath;
    } launchData = {nullptr};

    Handle(
        false, ArgOption::OPT_UNKNOWN,
        [](ArgOption option, const char* value, void* userData) -> bool {
            LaunchData* launchData = static_cast<LaunchData*>(userData);

            switch (option) {
            case ArgOption::OPT_LAUNCH:
                launchData->ready = true;
                launchData->launchPath = value;
                break;

            case ArgOption::OPT_RIIVO_XML:
                launchData->hasRiivoXml = true;
                break;

            case ArgOption::OPT_PATCH_ID:
                launchData->ready = true;
                launchData->hasPatchId = true;
                break;

            default:
                break;
            }

            return true;
        },
        &launchData
    );

    if (!launchData.ready) {
        PRINT(Patcher, ERROR, "Launch called without enough arguments");
        return;
    }

    PatchManager patchManager;

    if (launchData.hasPatchId) {
        if (launchData.hasRiivoXml) {
            Handle(
                false, ArgOption::OPT_RIIVO_XML,
                [&patchManager](ArgOption option, const char* value) -> bool {
                    if (!patchManager.LoadRiivolutionXML(value)) {
                        PRINT(
                            Patcher, ERROR,
                            "Failed to load Riivolution XML path '%s'", value
                        );
                        return false;
                    }
                }
            )
        } else {
            // No Riivolution XML path specified, load the default paths
            patchManager.LoadRiivolutionXML("/riivolution");
            patchManager.LoadRiivolutionXML("/apps/riivolution");
        }
    }
}
