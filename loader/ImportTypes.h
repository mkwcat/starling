#pragma once

#include <Util.h>

EXTERN_C_START

enum TwmImportType {
    TWM_FUNCTION_IMPORT,
    TWM_FUNCTION_REPLACE,
    TWM_DATA_IMPORT,
};

typedef struct {
    unsigned int address;
    unsigned int stub;
    TwmImportType type;
} TwmImportEntry;

EXTERN_C_END
