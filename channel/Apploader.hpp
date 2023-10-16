#pragma once

class Apploader
{
public:
    static Apploader* Instance()
    {
        static Apploader instance;
        return &instance;
    }

    Apploader(const Apploader&) = delete;
    Apploader() = default;

    void Load();
};
