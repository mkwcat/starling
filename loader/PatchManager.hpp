#pragma once

class PatchManager
{
public:
    /**
     * Load one or multiple Riivolution XML files.
     * @param path Path to the XML file or directory containing XML files.
     */
    static bool LoadRiivolutionXML(const char* path);
};
