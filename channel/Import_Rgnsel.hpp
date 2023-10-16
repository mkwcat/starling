#pragma once

#include <Import.h>
#include <Import_NW4R.hpp>

IMPORT_DECL(0x8000C9DC);
IMPORT_DECL(0x8000CB08);
IMPORT_DECL(0x8000CBD4);
IMPORT_DECL(0x8000CD48);
IMPORT_DECL(0x8000CDBC);
IMPORT_DECL(0x8000CDD0);

namespace Rgnsel
{

IMPORT(0x8000C984) //
void SetPaneText(nw4r::lyt::Layout* layout, const char* pane, u16* text);

class Pointer
{
public:
    IMPORT_ATTR(0x8000C9DC)
    Pointer();

    IMPORT_ATTR(0x8000CB08)
    void Init(nw4r::lyt::ArcResourceAccessor* arc);

    IMPORT_ATTR(0x8000CBD4)
    void Calc(
        u32 player, nw4r::lyt::DrawInfo* drawInfo, float x, float y, float rot
    );

    IMPORT_ATTR(0x8000CD48)
    void Draw(nw4r::lyt::DrawInfo* drawInfo);

    IMPORT_ATTR(0x8000CDBC)
    void SetEnabled(u32 player);

    IMPORT_ATTR(0x8000CDD0)
    void SetDisabled(u32 player);

private:
    FILL(0x0, 0x20);
};

} // namespace Rgnsel
