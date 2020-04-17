#pragma once

// Taken from https://github.com/reactos/reactos/blob/master/sdk/include/reactos/rossym.h

typedef struct _ROSSYM_HEADER
{
    unsigned long SymbolsOffset;
    unsigned long SymbolsLength;
    unsigned long StringsOffset;
    unsigned long StringsLength;
} ROSSYM_HEADER, *PROSSYM_HEADER;

typedef struct _ROSSYM_ENTRY
{
    ULONG_PTR Address;
    ULONG FunctionOffset;
    ULONG FileOffset;
    ULONG SourceLine;
} ROSSYM_ENTRY, *PROSSYM_ENTRY;

