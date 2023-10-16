// AES.hpp - AES engine interface
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <IOS.hpp>
#include <Types.h>
#include <Util.h>

class AES
{
public:
    static AES* s_instance;

private:
    enum class AESIoctl {
        Encrypt = 2,
        Decrypt = 3,
    };

public:
    /**
     * AES-128 CBC encrypt a block using the AES hardware engine.
     * @param[in] key 128-bit AES encryption key.
     * @param[in,out] iv 128-bit AES initialization vector.
     * @param[in] input Pointer to input data.
     * @param[in] size Input/output data size in bytes. Cannot be larger than
     * 0x10000.
     * @param[out] output Pointer to output data. Can be the same as the input
     * pointer.
     */
    s32
    Encrypt(const u8* key, u8* iv, const void* input, u32 size, void* output)
    {
        IOS::IOVector<2, 2> vec;
        vec.in[0].data = input;
        vec.in[0].len = size;
        vec.in[1].data = key;
        vec.in[1].len = 16;
        vec.out[0].data = output;
        vec.out[0].len = size;
        vec.out[1].data = iv;
        vec.out[1].len = 16;
        return m_rm.Ioctlv(AESIoctl::Encrypt, vec);
    }

    /**
     * AES-128 CBC decrypt a block using the AES hardware engine.
     * @param[in] key 128-bit AES encryption key.
     * @param[in,out] iv 128-bit AES initialization vector.
     * @param[in] input Pointer to input data.
     * @param[in] size Input/output data size in bytes. Cannot be larger than
     * 0x10000.
     * @param[out] output Pointer to output data. Can be the same as the input
     * pointer.
     */
    s32
    Decrypt(const u8* key, u8* iv, const void* input, u32 size, void* output)
    {
        IOS::IOVector<2, 2> vec;
        vec.in[0].data = input;
        vec.in[0].len = size;
        vec.in[1].data = key;
        vec.in[1].len = 16;
        vec.out[0].data = output;
        vec.out[0].len = size;
        vec.out[1].data = iv;
        vec.out[1].len = 16;
        return m_rm.Ioctlv(AESIoctl::Decrypt, vec);
    }

private:
    IOS::ResourceCtrl<AESIoctl> m_rm{"/dev/aes"};
};
