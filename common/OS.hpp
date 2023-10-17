// OS.hpp - PPC and IOS compatible types and functions
//   Written by Palapeli
//
// SPDX-License-Identifier: GPL-2.0-only

#pragma once

#include <Types.h>
#include <Util.h>
#include <bit>
#include <cassert>

#ifdef TARGET_IOS
#  include <Syscalls.h>
#  include <System.hpp>
#else
#  include <Import_RVL_OS.h>
#endif

#include <new>

#ifdef TARGET_IOS

// IOS MEM1 base. For some reason they thought it was a good idea to leave this
// at 0. C++ doesn't like this in some places I think.
#  define MEM1_BASE ((void*) 0x00000000)

#else

// PPC cached MEM1 base
#  define MEM1_BASE ((void*) 0x80000000)

#  define MEM_CACHED_BASE ((void*) 0x80000000)
#  define MEM_UNCACHED_BASE ((void*) 0xC0000000)

#endif

template <class From>
concept QueueConvertible =
    requires { std::bit_cast<u32>(std::declval<From>()); };

#ifdef TARGET_IOS

/* IOS implementation */
template <typename T, u32 TCount>
class IOS_Queue
{
public:
    IOS_Queue(const IOS_Queue& from) = delete;

    explicit IOS_Queue()
    {
        const s32 ret = IOS_CreateMessageQueue(m_base, TCount);
        m_id = ret;
        assert(ret >= 0);
    }

    ~IOS_Queue()
    {
        if (!QueueConvertible<T>) {
            T* msg = nullptr;
            while (IOS_ReceiveMessage(m_id, (u32*) (&msg), 1) == IOS_ERROR_OK) {
                delete msg;
            }
        }

        const s32 ret = IOS_DestroyMessageQueue(m_id);
        assert(ret == IOS_ERROR_OK);
    }

    void Send(const T& msg)
        requires(!QueueConvertible<T>)
    {
        const s32 ret =
            IOS_SendMessage(m_id, reinterpret_cast<u32>(new T(msg)), 0);
        assert(ret == IOS_ERROR_OK);
    }

    void Send(T msg)
        requires(QueueConvertible<T>)
    {
        const s32 ret = IOS_SendMessage(m_id, std::bit_cast<u32>(msg), 0);
        assert(ret == IOS_ERROR_OK);
    }

    T Receive()
        requires(!QueueConvertible<T>)
    {
        u32 msgV;
        const s32 ret = IOS_ReceiveMessage(m_id, &msgV, 0);
        assert(ret == IOS_ERROR_OK);

        assert(msgV != 0);
        T* ptr = reinterpret_cast<T*>(msgV);

        T out = *ptr;
        delete ptr;
        return out;
    }

    T Receive()
        requires(QueueConvertible<T>)
    {
        u32 msgV;
        const s32 ret = IOS_ReceiveMessage(m_id, &msgV, 0);
        assert(ret == IOS_ERROR_OK);

        return std::bit_cast<T>(msgV);
    }

    s32 GetID() const
    {
        return m_id;
    }

private:
    u32 m_base[TCount] = {};
    s32 m_id = IOS_ERROR_INVALID;
};

template <typename T, u32 TCount = 8>
using Queue = IOS_Queue<T, TCount>;

#else

/* PPC implementation */

#endif

#ifdef TARGET_IOS

// TODO: Create recursive IOS mutex
class IOS_Mutex
{
public:
    IOS_Mutex(const IOS_Mutex&) = delete;

    IOS_Mutex()
    {
        m_queue.Send(0);
    }

    void Lock()
    {
        m_queue.Receive();
    }

    void Unlock()
    {
        m_queue.Send(0);
    }

private:
    Queue<u32, 1> m_queue;
};

using Mutex = IOS_Mutex;

#else

class PPC_Mutex
{
public:
    PPC_Mutex(const PPC_Mutex&) = delete;

    PPC_Mutex()
    {
        OSInitMutex(&m_mutex);
    }

    void Lock()
    {
        OSLockMutex(&m_mutex);
    }

    void Unlock()
    {
        OSUnlockMutex(&m_mutex);
    }

private:
    OSMutex m_mutex;
};

using Mutex = PPC_Mutex;

#endif

class ScopeLock
{
public:
    ScopeLock(const ScopeLock&) = delete;

    explicit ScopeLock(Mutex& mutex)
    {
        m_mutex = &mutex;
        m_mutex->Lock();
    }

    ~ScopeLock()
    {
        m_mutex->Unlock();
    }

private:
    Mutex* m_mutex;
};

#ifdef TARGET_IOS

class IOS_Thread
{
public:
    typedef s32 (*Proc)(void* arg);

    IOS_Thread(const IOS_Thread&) = delete;

    IOS_Thread()
    {
        m_arg = nullptr;
        f_proc = nullptr;
        m_valid = false;
        m_tid = -1;
        m_ownedStack = nullptr;
    }

    IOS_Thread(s32 thread)
    {
        m_arg = nullptr;
        f_proc = nullptr;
        m_tid = thread;
        if (m_tid >= 0)
            m_valid = true;
        m_ownedStack = nullptr;
    }

    IOS_Thread(Proc proc, void* arg, u8* stack, u32 stackSize, s32 prio)
    {
        m_arg = nullptr;
        f_proc = nullptr;
        m_valid = false;
        m_tid = -1;
        m_ownedStack = nullptr;
        create(proc, arg, stack, stackSize, prio);
    }

    ~IOS_Thread()
    {
        if (m_ownedStack != nullptr)
            delete m_ownedStack;
    }

    void create(Proc proc, void* arg, u8* stack, u32 stackSize, s32 prio)
    {
        f_proc = proc;
        m_arg = arg;

        if (stack == nullptr) {
            stack = new ((std::align_val_t) 32) u8[stackSize];
            m_ownedStack = stack;
        }
        u32* stackTop = reinterpret_cast<u32*>(stack + stackSize);

        m_ret = IOS_CreateThread(
            __threadProc, reinterpret_cast<void*>(this), stackTop, stackSize,
            prio, true
        );
        if (m_ret < 0)
            return;

        m_tid = m_ret;
        m_ret = IOS_StartThread(m_tid);
        if (m_ret < 0)
            return;

        m_valid = true;
    }

    static s32 __threadProc(void* arg)
    {
        IOS_Thread* thr = reinterpret_cast<IOS_Thread*>(arg);
        if (thr->f_proc != nullptr)
            thr->f_proc(thr->m_arg);
        return 0;
    }

    s32 id() const
    {
        return this->m_tid;
    }

    s32 getError() const
    {
        return this->m_ret;
    }

protected:
    void* m_arg;
    Proc f_proc;
    bool m_valid;
    s32 m_tid;
    s32 m_ret;
    u8* m_ownedStack;
};

using Thread = IOS_Thread;

#else

/**
 * Thread implementation for PPC, using OSThread.
 */
class PPC_Thread
{
public:
    typedef void (*Proc)(void* arg);

    PPC_Thread(const PPC_Thread&) = delete;

    /**
     * Default Thread constructor. Call Create to manually create the thread.
     */
    PPC_Thread()
      : m_arg(nullptr)
      , m_proc(nullptr)
      , m_valid(false)
      , m_ownedStack(nullptr)
      , m_ownedStackSize(0)
      , m_thread()
    {
    }

    /**
     * Default Thread destructor. This will wait for the thread to finish.
     */
    ~PPC_Thread()
    {
        bool currentThread = m_valid && OSGetCurrentThread() == &m_thread;

        if (m_valid && !currentThread) {
            // TODO: Test if this actually works. The intention is to wait until
            // the thread has willingly exited.
            OSResumeThread(&m_thread);
            OSJoinThread(&m_thread, nullptr);
        }

        m_valid = false;

        if (m_ownedStack != nullptr) {
            delete m_ownedStack;
            m_ownedStack = nullptr;
        }

        if (currentThread) {
            OSCancelThread(&m_thread);
        }
    }

    /**
     * Thread constructor. Automatically calls Create.
     * @param proc Thread entry point. Must be of type s32 Name(void* arg).
     * @param arg Optional argument to pass to the thread. Usually used to
     * supply a pointer to an object.
     * @param stack Pointer to the _BOTTOM_ of the stack, of size stackSize. If
     * provided as nullptr the stack will be allocated.
     * @param prio Thread priority.
     * @param autoRun Setting to true will automatically resume the thread after
     * creating it.
     *
     * Call IsValid() to check if the thread was successfully created.
     */
    PPC_Thread(
        Proc proc, void* arg = nullptr, u8* stack = nullptr,
        u32 stackSize = 0x8000, s32 prio = 10, bool autoRun = true
    )
      : PPC_Thread()
    {
        Create(proc, arg, stack, stackSize, prio, autoRun);
    }

    /**
     * Create the thread and automatically start it.
     * @param proc Thread entry point. Must be of type s32 Name(void* arg).
     * @param arg Optional argument to pass to the thread. Usually used to
     * supply a pointer to an object.
     * @param stack Pointer to the _BOTTOM_ of the stack, of size stackSize. If
     * provided as nullptr the stack will be allocated.
     * @param prio Thread priority.
     * @param autoRun Setting to true will automatically resume the thread after
     * creating it.
     *
     * Call IsValid() to check if the thread was successfully created.
     */
    void Create(
        Proc proc, void* arg = nullptr, u8* stack = nullptr,
        u32 stackSize = 0x8000, s32 prio = 10, bool autoRun = true
    )
    {
        // Return if the thread is already valid
        if (m_valid) {
            return;
        }

        m_proc = proc;
        m_arg = arg;

        if (stack == nullptr) {
            // Delete the old stack to prevent a memory leak, but reuse it if
            // the size matches
            if (m_ownedStack == nullptr || m_ownedStackSize != stackSize) {
                if (m_ownedStack != nullptr) {
                    delete m_ownedStack;
                    m_ownedStack = nullptr;
                }

                m_ownedStack = new (std::align_val_t(32)) u8[stackSize];
            }

            stack = m_ownedStack;
        }

        u32* stackTop = reinterpret_cast<u32*>(stack + stackSize);

        if (!OSCreateThread(
                &m_thread, ThreadProc, reinterpret_cast<void*>(this), stackTop,
                stackSize, prio, 0
            )) {
            return;
        }

        if (autoRun) {
            OSResumeThread(&m_thread);
        }

        m_valid = true;
    }

    /**
     * Start executing on the thread.
     */
    void Start()
    {
        OSResumeThread(&m_thread);
    }

    /**
     * Force abort the thread.
     */
    void Cancel()
    {
        m_valid = false;
        OSCancelThread(&m_thread);
    }

    /**
     * Get the internal OSThread struct.
     * @returns Pointer to OSThread, or nullptr if the thread is invalid.
     */
    OSThread* GetOSThread()
    {
        if (!m_valid) {
            return nullptr;
        }

        return &m_thread;
    }

    /**
     * Check if the thread was successfully created.
     */
    bool IsValid() const
    {
        return m_valid;
    }

private:
    static void* ThreadProc(void* arg)
    {
        PPC_Thread* thread = reinterpret_cast<PPC_Thread*>(arg);
        if (thread->m_proc != nullptr) {
            thread->m_proc(thread->m_arg);
        }

        return nullptr;
    }

private:
    void* m_arg;
    Proc m_proc;
    bool m_valid;

    u8* m_ownedStack;
    u32 m_ownedStackSize;

    OSThread m_thread;
};

using Thread = PPC_Thread;

#endif
