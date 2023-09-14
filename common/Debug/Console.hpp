#pragma once

#include <System/Types.h>

class Console
{
public:
#ifdef TARGET_PPC

    /**
     * Initalize and display the debug console.
     */
    static void Init();

    /**
     * Configure VI for the debug console.
     * @param clear Clear the XFB.
     */
    static void ConfigureVideo(bool clear = false);

#endif

    /**
     * Reinitialize the console after reloading to a new instance.
     */
    static void Reinit();

    /**
     * Get the width of the console framebuffer.
     */
    static u16 GetXFBWidth();

    /**
     * Get the height of the console framebuffer.
     */
    static u16 GetXFBHeight();

    /**
     * Read from the specified pixel on the framebuffer.
     */
    static u8 ReadGrayscaleFromXFB(u16 x, u16 y);

    /**
     * Write to the specified pixel on the framebuffer.
     */
    static void WriteGrayscaleToXFB(u16 x, u16 y, u8 intensity);

    /**
     * Move the framebuffer up by the specified height.
     */
    static void MoveUp(u16 height);

    /**
     * Flush the XFB to main memory after writing to it.
     */
    static void FlushXFB();

    /**
     * Print a string to the debug console.
     */
    static void Print(const char* s);
};
