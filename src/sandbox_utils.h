#pragma once

#include <string.h>
#include "sandbox_defs.h"

inline b32
StringEquals(const wchar *Str1, const wchar *Str2) {
    b32 Result = wcscmp(Str1, Str2) == 0;
    return Result;
}

inline wchar *
GetLastAfterDelimiter(wchar *String, const wchar Delimiter) {
    wchar *Result = String;

    for (wchar *Scan = String; *Scan; ++Scan) {
        if (*Scan == Delimiter) {
            Result = Scan + 1;
        }
    }

    return Result;
}

inline u32
StringLength(const wchar *String) {
    u32 Result = (u32)wcslen(String);

    return Result;
}

inline void
ConcatenateStrings(const wchar *SourceA, const wchar *SourceB, wchar *Dest) {
    u32 SourceALength = StringLength(SourceA);
    for (u32 Index = 0; Index < SourceALength; ++Index) {
        *Dest++ = *SourceA++;
    }

    u32 SourceBLength = StringLength(SourceB);
    for (u32 Index = 0; Index < SourceBLength; ++Index) {
        *Dest++ = *SourceB++;
    }

    *Dest++ = 0;
}

inline void
CopyString(const wchar *Source, wchar *Dest, u32 DestLength)
{
    wcscpy_s(Dest, DestLength, Source);
}
