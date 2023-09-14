#pragma once

#include <Boot/Init.hpp>
#include <Import/RVL_OS.h>
#include <Import/Twm.h>
#include <System/Types.h>

TwmImportDecl(0x80059B00);
TwmImportDecl(0x80059B94);
TwmImportDecl(0x8005A1CC);
TwmImportDecl(0x8005E3A0);
TwmImportDecl(0x8005F478);
TwmImportDecl(0x8005F4BC);
TwmImportDecl(0x80067700);
TwmImportDecl(0x8006770C);
TwmImportDecl(0x800673A8);
TwmImportDecl(0x8006825C);
TwmImportDecl(0x8006829C);
TwmImportDecl(0x800682F8);
TwmImportDecl(0x800683A8);

namespace nw4r
{

namespace math
{

struct MTX34 {
    float mtx[3][4];
};

static_assert(sizeof(MTX34) == 0x30);

TwmImport(0x8005DF58) //
  float Atan2FIdx(float x, float y);

struct VEC2 {
    VEC2() = default;

    VEC2(float x2, float y2)
    {
        x = x2;
        y = y2;
    }

    union {
        struct {
            float x;
            float y;
        };

        float f[2];
    };
};

static_assert(sizeof(VEC2) == 0x8);

struct VEC3 {
    VEC3() = default;

    VEC3(float x2, float y2, float z2)
    {
        x = x2;
        y = y2;
        z = z2;
    }

    union {
        struct {
            float x;
            float y;
            float z;
        };

        float f[3];
    };
};

static_assert(sizeof(VEC3) == 0xC);

} // namespace math

namespace ut
{

struct Color {
    union {
        struct {
            u8 r, g, b, a;
        };

        u32 rgba;
    };
};

struct Rect {
    Rect() = default;

    Rect(float left2, float top2, float right2, float bottom2)
    {
        left = left2;
        top = top2;
        right = right2;
        bottom = bottom2;
    }

    union {
        struct {
            float left;
            float top;
            float right;
            float bottom;
        };

        float f[4];
    };
};

class ArchiveFont
{
    FILL(0x0, 0x1C);

public:
    TwmImportAttr(0x80059B00) //
      ArchiveFont();

    TwmImportAttr(0x80059B94) //
      static u32 GetRequireBufferSize(const void* brfnt, const char* param_2);

    TwmImportAttr(0x8005A1CC) //
      void Construct(
        void* buffer, u32 bufferSize, const void* brfnt, const char* param_4);
};

static_assert(sizeof(ArchiveFont) == 0x1C);

} // namespace ut

namespace lyt
{

class FontRefLink
{
    FILL(0x0, 0x8C);

public:
    TwmImportAttr(0x8006825C) //
      void Set(const char* name, nw4r::ut::ArchiveFont* font);
};

static_assert(sizeof(FontRefLink) == 0x8C);

class ArcResourceAccessor
{
public:
    TwmImportAttr(0x8006829C) //
      ArcResourceAccessor();

    ~ArcResourceAccessor();

    TwmImportAttr(0x800682F8) //
      bool Attach(const void* arc, const char* root);

    TwmImportAttr(0x800683A8) //
      void RegistFont(FontRefLink* font);

    virtual void _0()
    {
    }

    virtual void _1()
    {
    }

    virtual void __dt(int type);

    virtual void* GetResource(u32 type, const char* name, u32* size);

    FILL(0x4, 0xB0);
};

static_assert(sizeof(ArcResourceAccessor) == 0xB0);

constexpr u32 ARC_TYPE_BLYT = 0x626C7974;
constexpr u32 ARC_TYPE_ANIM = 0x616E696D;
constexpr u32 ARC_TYPE_MISC = 0x6D697363;

class DrawInfo
{
public:
    TwmImportAttr(0x800673A8) //
      DrawInfo();

    void SetViewMtx(math::MTX34 mtx)
    {
        m_viewMtx = mtx;
    }

    void SetViewRect(ut::Rect rect)
    {
        m_viewRect = rect;
    }

    virtual void _0()
    {
    }

    virtual void _1()
    {
    }

    virtual void __dt(int type);

    math::MTX34 m_viewMtx;
    ut::Rect m_viewRect;
    FILL(0x44, 0x54);
};

static_assert(sizeof(DrawInfo) == 0x54);

class AnimTransform
{
public:
    TwmImportAttr(0x80067700) //
      u16 GetFrameSize() const;

    TwmImportAttr(0x8006770C) //
      bool IsLoopData() const;

    float GetFrame() const
    {
        return m_frame;
    }

    void SetFrame(float frame)
    {
        m_frame = frame;
    }

private:
    FILL(0x0, 0x10);
    float m_frame;
};

static_assert(sizeof(AnimTransform) == 0x14);

struct Size {
    float width;
    float height;
};

class Pane

{
public:
    // CW compatibility
    virtual void VT_0x00()
    {
    }

    virtual void VT_0x04()
    {
    }

    virtual void __dt(int type);

    virtual void VT_0x0C();
    virtual void VT_0x10();
    virtual void VT_0x14();
    virtual void VT_0x18();
    virtual void VT_0x1C();
    virtual void VT_0x20();
    virtual void VT_0x24();
    virtual void VT_0x28();
    virtual void VT_0x2C();
    virtual void VT_0x30();
    virtual void VT_0x34();
    virtual void VT_0x38();

    virtual Pane* FindPaneByName(const char* name, bool recursive = true);

    math::VEC3 GetPosition() const
    {
        return m_position;
    }

    void SetPosition(math::VEC3 pos)
    {
        m_position = pos;
    }

    void SetScale(math::VEC2 scale)
    {
        m_scale = scale;
    }

    Size GetSize() const
    {
        return m_size;
    }

    void SetSize(Size size)
    {
        m_size = size;
    }

    u8 GetBasePositionH() const
    {
        return m_basePosition % 3;
    }

    u8 GetBasePositionV() const
    {
        return m_basePosition / 3;
    }

    const math::MTX34 GetGlobalMtx() const
    {
        return m_globalMtx;
    }

    TwmImportAttr(0x8005E3A0) //
      ut::Rect GetPaneRect(const DrawInfo& drawInfo) const;

protected:
    FILL(0x4, 0x2C);

    math::VEC3 m_position;
    math::VEC3 m_rotation;
    math::VEC2 m_scale;
    Size m_size;

    math::MTX34 m_mtx;
    math::MTX34 m_globalMtx;

    /* 0xB4 */ char m_name[17];

    FILL(0xC5, 0xCD);

    u8 m_alpha;
    u8 m_globalAlpha;
    u8 m_basePosition;
};

class Layout
{
public:
    Layout(Layout&) = delete;

    TwmImportAttr(0x8005F478) //
      Layout();

    ~Layout();

    static void SetAllocator(MEMAllocator* allocator)
    {
        if (VER_KOREA) {
            *(MEMAllocator**) 0x8023DA48 = allocator;
        } else {
            *(MEMAllocator**) 0x8024A948 = allocator;
        }
    }

    // CW compatibility
    virtual void _0()
    {
    }

    virtual void _1()
    {
    }

    TwmImportAttr(0x8005F4BC) //
      virtual void __dt(int type);

    virtual bool Build(const void* brlyt, ArcResourceAccessor* arc);
    virtual AnimTransform* CreateAnimTransform(
      const void* brlan, ArcResourceAccessor* arc);
    virtual void BindAnimation(AnimTransform* anim);
    virtual void UnbindAnimation(AnimTransform* anim);
    virtual void _7();
    virtual void _8();
    virtual void CalculateMtx(DrawInfo* drawInfo);
    virtual void Draw(DrawInfo* drawInfo);
    virtual void Animate();

    Pane* GetRootPane() const
    {
        return m_rootPane;
    }

    // More TODO

protected:
    FILL(0x4, 0x10);
    Pane* m_rootPane;
    FILL(0x14, 0x20);
};

static_assert(sizeof(Layout) == 0x20);

extern "C" {

// Extremely disgusting but I can't think of a good way around this

__attribute__((weak)) Layout* _ZN4nw4r3lyt6LayoutD1Ev(Layout* obj)
{
    obj->__dt(0);
    return obj;
}

__attribute__((weak)) Layout* _ZN4nw4r3lyt6LayoutD2Ev(Layout* obj)
{
    obj->__dt(1);
    return obj;
}

//
}

} // namespace lyt

} // namespace nw4r
