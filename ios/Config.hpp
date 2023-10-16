// Config.hpp - Saoirse config
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

// Config is currently hardcoded

class Config
{
public:
    static Config* s_instance;

    bool IsISFSPathReplaced(const char* path);
    bool IsFileLogEnabled();
    bool BlockIOSReload();
};
