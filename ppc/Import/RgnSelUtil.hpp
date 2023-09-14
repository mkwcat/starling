#pragma once

#include <Import/NW4R.hpp>
#include <Import/Twm.h>

namespace RgnSelUtil
{

TwmImport(0x8000C984) //
  void SetPaneText(nw4r::lyt::Layout* layout, const char* pane, u16* text);

TwmImportDecl(0x8000C9DC);
TwmImportDecl(0x8000CB08);
TwmImportDecl(0x8000CBD4);
TwmImportDecl(0x8000CD48);
TwmImportDecl(0x8000CDBC);
TwmImportDecl(0x8000CDD0);

class Pointer
{
public:
    TwmImportAttr(0x8000C9DC) //
      Pointer();

    TwmImportAttr(0x8000CB08) //
      void Init(nw4r::lyt::ArcResourceAccessor* arc);

    TwmImportAttr(0x8000CBD4) //
      void Calc(
        u32 player, nw4r::lyt::DrawInfo* drawInfo, float x, float y, float rot);

    TwmImportAttr(0x8000CD48) //
      void Draw(nw4r::lyt::DrawInfo* drawInfo);

    TwmImportAttr(0x8000CDBC) //
      void SetEnabled(u32 player);

    TwmImportAttr(0x8000CDD0) //
      void SetDisabled(u32 player);

private:
    FILL(0x0, 0x20);
};

} // namespace RgnSelUtil
