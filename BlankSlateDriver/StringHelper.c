#include"StringHelper.h"
#include"MemoryHelper.h"
NTSTATUS SearchPattern(IN PUCHAR PatternValue, IN UCHAR Keyword,
	IN ULONG_PTR PatternValueLength, IN const VOID* VirtualBase, IN ULONG_PTR VirtualSize, OUT PVOID* Found)
{
	if (Found == NULL || PatternValue == NULL || VirtualBase == NULL)
		return STATUS_INVALID_PARAMETER;
	for (ULONG_PTR i = 0; i < VirtualSize - PatternValueLength; i++)
	{
		BOOLEAN IsOk = TRUE;
		for (ULONG_PTR j = 0; j < PatternValueLength; j++)
		{
			if (PatternValue[j] != Keyword && PatternValue[j] != ((PUCHAR)VirtualBase)[i + j])
			{
				IsOk = FALSE;
				break;
			}
		}
		if (IsOk != FALSE)
		{
			*Found = (PUCHAR)VirtualBase + i;
			return STATUS_SUCCESS;
		}
			
	}
		
	return STATUS_NOT_FOUND;
}


ULONG FindKey(PUCHAR PatternValue, ULONG PatternValueLength, PUCHAR VirtualAddress, ULONG ViewSize)
{
	ULONG i = 0;
	PUCHAR v1 = VirtualAddress;
	UCHAR v5[0x100];
	if (ViewSize <= 0)
		return -1;
	memset(v5, PatternValueLength + 1, 0x100);
	for (i = 0; i < PatternValueLength; i++)
	{
		v5[PatternValue[i]] = (UCHAR)(PatternValueLength - i);
	}
			
	while (v1 + PatternValueLength <= VirtualAddress + ViewSize)
	{
		UCHAR* m = PatternValue, *n = v1;
		ULONG i = 0;
		for (i = 0; i < PatternValueLength; i++)
		{
			if (m[i] != n[i])
				break;
		}
			
		if (i == PatternValueLength)
			return (ULONG)v1;
		if (v1 + PatternValueLength == VirtualAddress + ViewSize)
			return -1;
		v1 += v5[v1[PatternValueLength]];
	}
	return -1;
}



NTSTATUS UnicodeStringCopy2UnicodeString(OUT PUNICODE_STRING DestinationString, IN PUNICODE_STRING SourceString)
{
	ASSERT(DestinationString != NULL && SourceString != NULL);
	if (DestinationString == NULL || SourceString == NULL || SourceString->Buffer == NULL)
		return STATUS_INVALID_PARAMETER;
	if (SourceString->Length == 0)
	{
		DestinationString->Length = DestinationString->MaximumLength = 0;
		DestinationString->Buffer = NULL;
		return STATUS_SUCCESS;
	}
	DestinationString->Buffer = AllocatePoolWithTag(PagedPool, SourceString->MaximumLength);
	DestinationString->Length = SourceString->Length;
	DestinationString->MaximumLength = SourceString->MaximumLength;
	memcpy(DestinationString->Buffer, SourceString->Buffer, SourceString->Length+2);
	return STATUS_SUCCESS;
}
	

ULONG CmpAndGetStringLength(PUNICODE_STRING UnicodeString, ULONG Length)
{
	ULONG v1 = 0;
	v1 = Length > UnicodeString->Length / sizeof(WCHAR) ? UnicodeString->Length / sizeof(WCHAR) : Length - 1;
	return v1;
}
	
