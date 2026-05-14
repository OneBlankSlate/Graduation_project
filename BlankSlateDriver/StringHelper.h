#pragma once
#include<fltKernel.h>

NTSTATUS SearchPattern(IN PUCHAR PatternValue, IN UCHAR Keyword,IN ULONG_PTR PatternValueLength, IN const VOID* VirtualBase, IN ULONG_PTR VirtualSize, OUT PVOID* Found);  //模糊匹配
NTSTATUS UnicodeStringCopy2UnicodeString(OUT PUNICODE_STRING DestinationString, IN PUNICODE_STRING SourceString);
ULONG FindKey(PUCHAR PatternValue, ULONG PatternValueLength, PUCHAR VirtualAddress, ULONG ViewSize);  //完全匹配
ULONG CmpAndGetStringLength(PUNICODE_STRING UnicodeString, ULONG Length);
