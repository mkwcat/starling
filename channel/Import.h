#pragma once

#include <ImportTypes.h>

#define _TwmTableID(_COUNT) __TWM_TABLE##_COUNT
#define _TwmStubID(_COUNT) __TWM_STUB##_COUNT

#define _TwmCreateFImportStub(_COUNT, _ADDRESS)                                \
    void _TwmStubID(_COUNT)() asm("__TWM_STUB_" #_ADDRESS);                    \
                                                                               \
    __attribute__((__section__(".twm_table." #_ADDRESS), __weak__))            \
    TwmImportEntry _TwmTableID(_COUNT) asm("__TWM_TABLE_" #_ADDRESS) = {       \
        .address = _ADDRESS,                                                   \
        .stub = (unsigned int) &_TwmStubID(_COUNT),                            \
        .type = TWM_FUNCTION_IMPORT,                                           \
    };                                                                         \
                                                                               \
    __attribute__((__weak__)) void _TwmStubID(_COUNT)()                        \
    { /* A reference to the entry so garbage collect won't strip it */         \
        asm volatile("lwz 0, __TWM_TABLE_" #_ADDRESS "@l(0)");                 \
        __builtin_unreachable();                                               \
    }

#define IMPORT_DECL(_ADDRESS) _TwmCreateFImportStub(__COUNTER__, _ADDRESS)

#define IMPORT_ATTR(_ADDRESS)                                                  \
    __attribute__((alias("__TWM_STUB_" #_ADDRESS), __weak__))

#define IMPORT(_ADDRESS)                                                       \
    IMPORT_DECL(_ADDRESS);                                                     \
    IMPORT_ATTR(_ADDRESS)

#define _TwmCreateFReplaceStub(_COUNT, _ADDRESS)                               \
    void _TwmStubID(_COUNT)() asm("__TWM_STUB_" #_ADDRESS);                    \
                                                                               \
    __attribute__((__section__(".twm_table_strong")))                          \
    TwmImportEntry _TwmTableID(_COUNT) asm("__TWM_TABLE_" #_ADDRESS) = {       \
        .address = _ADDRESS,                                                   \
        .stub = (unsigned int) &_TwmStubID(_COUNT),                            \
        .type = TWM_FUNCTION_REPLACE,                                          \
    };                                                                         \
                                                                               \
    /* Generate an empty symbol that's guaranteed to be located just before    \
     * our next function that we want the pointer to, because you can't just   \
     * take an address of a function in C++. */                                \
    __asm__("    .section \".twm_replace." #_ADDRESS "\", \"ax\"\n"            \
            "    .global  __TWM_STUB_" #_ADDRESS "\n"                          \
            "    .p2align 2\n"                                                 \
            "__TWM_STUB_" #_ADDRESS ":\n"                                      \
            "    .size    __TWM_STUB_" #_ADDRESS ", 0\n");

#define REPLACE(_ADDRESS)                                                      \
    _TwmCreateFReplaceStub(__COUNTER__, _ADDRESS)                              \
        __attribute__((section(".twm_replace." #_ADDRESS)))
