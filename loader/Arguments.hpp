#pragma once

#include <Types.h>

class Arguments
{
public:
#define COMMAND_LINE_OPTIONS                                                   \
    X(OPT_LAUNCH, "--launch", true,                                            \
      "Defines a path to the title to directly launch. Providing this will "   \
      "skip loading the channel and directly launch the game. Passing "        \
      "/dev/di will launch the game from the disc (a specific partition can "  \
      "be specified by standard name like /dev/di/CHANNEL or /dev/di/P-HBLE, " \
      "or by number like /dev/di/2). A NAND title can be launched by passing " \
      "the path to the TMD file (e.g. "                                        \
      "/title/00010001/57444d45/content/title.tmd). A game can be launched "   \
      "from an external storage device by prefixing the path with /mnt/sd, "   \
      "/mnt/usb0, /mnt/usb1, etc. (e.g. /mnt/sd/games/SMNE01.rvz). The file "  \
      "type will be automatically deduced.")                                   \
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

    /**
     * Create a patcher arguments struct from command line arguments passed
     * using wiiload or through launching the title.
     */
    Arguments(const u32 argc, const char* const* argv)
      : m_argc(argc)
      , m_argv(argv)
    {
    }

    /**
     * Verify the arguments aren't malformed.
     */
    bool Validate() const;

    /**
     * Check if the arguments are complete enough to start the game.
     */
    bool IsStartReady() const;

    /**
     * Handle the command-line arguments. Set option to OPT_UNKNOWN to handle
     * all option types.
     */
    void Handle(
        bool printErrors, ArgOption handleOption,
        bool (*callback)(ArgOption option, const char* value, void* userData) =
            nullptr,
        void* userData = nullptr
    ) const;

    /**
     * Has the option been passed?
     */
    bool HasOption(ArgOption option) const;

    /**
     * Launch the game using the provided arguments.
    */
    void Launch();

private:
    /**
     * Get the option enum by its command-line name.
     */
    static ArgOption GetOptionByName(const char* name, const u32 nameLen);

    /**
     * Check if the option has a value.
     */
    static bool ArgOption_HasValue(ArgOption option);

private:
    const u32 m_argc;
    const char* const* m_argv;
};
