// SHA.hpp - SHA engine interface
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <IOS.hpp>
#include <Types.h>
#include <Util.h>

class SHA
{
public:
    static SHA* s_instance;

    struct Context {
        u32 state[5];
        u32 count[2];
    };

    enum class SHAIoctl {
        INIT = 0,
        UPDATE = 1,
        FINAL = 2,
    };

private:
    s32
    Command(SHAIoctl cmd, Context* ctx, const void* data, u32 len, u8* hashOut)
    {
        IOS::IOVector<1, 2> vec;
        vec.in[0].data = data;
        vec.in[0].len = len;
        vec.out[0].data = reinterpret_cast<void*>(ctx);
        vec.out[0].len = sizeof(Context);
        vec.out[1].data = hashOut;
        vec.out[1].len = hashOut ? 0x14 : 0;

        return m_rm.Ioctlv(cmd, vec);
    }

public:
    s32 Init(Context* ctx)
    {
        return Command(SHAIoctl::INIT, ctx, nullptr, 0, nullptr);
    }

    /**
     * Update hash in the SHA-1 context.
     */
    s32 Update(Context* ctx, const void* data, u32 len)
    {
        return Command(SHAIoctl::UPDATE, ctx, data, len, nullptr);
    }

    /**
     * Finalize the SHA-1 context and get the result hash.
     */
    s32 Final(Context* ctx, u8* hashOut)
    {
        return Command(SHAIoctl::FINAL, ctx, nullptr, 0, hashOut);
    }

    /**
     * Finalize the SHA-1 context and get the result hash.
     */
    s32 Final(Context* ctx, const void* data, u32 len, u8* hashOut)
    {
        if (!len)
            return Final(ctx, hashOut);

        return Command(SHAIoctl::FINAL, ctx, data, len, hashOut);
    }

    /**
     * Quick full hash calculate.
     */
    static s32 Calculate(const void* data, u32 len, u8* hashOut)
    {
        assert(s_instance != nullptr);

        Context ctx alignas(32);

        s32 ret = s_instance->Init(&ctx);
        if (ret != IOS::IOSError::OK)
            return ret;

        return s_instance->Final(&ctx, data, len, hashOut);
    }

private:
    IOS::ResourceCtrl<SHAIoctl> m_rm{"/dev/sha"};
};
