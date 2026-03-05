#pragma once
#include<fltKernel.h>

NTSTATUS SearchPattern(IN PUCHAR PatternValue, IN UCHAR Keyword,IN ULONG_PTR PatternValueLength, IN const VOID* VirtualBase, IN ULONG_PTR VirtualSize, OUT PVOID* Found);  //Ä£ºýÆ¥Åä
NTSTATUS UnicodeStringCopy2UnicodeString(OUT PUNICODE_STRING DestinationString, IN PUNICODE_STRING SourceString);
ULONG FindKey(PUCHAR PatternValue, ULONG PatternValueLength, PUCHAR VirtualAddress, ULONG ViewSize);  //ÍêÈ«Æ¥Åä
ULONG CmpAndGetStringLength(PUNICODE_STRING UnicodeString, ULONG Length);
