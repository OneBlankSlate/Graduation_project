#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"
#define MAX_LENGTH 20

typedef struct _COMMUNICATE_PROCESS_MEMORY_
{
	OPERATE_TYPE OperateType;
	struct
	{
		ULONG_PTR ProcessIdentity;
		PVOID BaseAddress;
		SIZE_T RegionSize;
		ULONG NewProtect;
		ULONG OldProtect;
	}Modify;
	union
	{
		struct {
			ULONG_PTR ProcessIdentity;
		}Query;
		struct {
			PVOID BaseAddress;
			SIZE_T RegionSize;
			ULONG_PTR ProcessIdentity;
		}Read;
		struct {
			PVOID BaseAddress;
			SIZE_T RegionSize;
			ULONG_PTR ProcessIdentity;
			char* BufferData;
		}Write;

	}ul;
}COMMUNICATE_PROCESS_MEMORY, * PCOMMUNICATE_PROCESS_MEMORY;
typedef struct _MEMORY_INFORMATION_ENTRY_
{
	PVOID BaseAddress;
	SIZE_T RegionSize;
	ULONG Protect;
	ULONG State;
	ULONG Type;
}MEMORY_INFORMATION_ENTRY, * PMEMORY_INFORMATION_ENTRY;
typedef struct _MEMORYS_INFORMATION_
{
	ULONG NumberOfMemory;
	MEMORY_INFORMATION_ENTRY MemoryInfo[1];
}MEMORYS_INFORMATION, * PMEMORYS_INFORMATION;


NTSYSAPI
NTSTATUS
NTAPI
ZwProtectVirtualMemory(
	HANDLE ProcessHandle,
	PVOID* BaseAddress,
	PSIZE_T RegionSize,
	ULONG NewProtect,
	PULONG OldProtect
);

NTSTATUS PsEnumProcessMem(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS EnumProcessMemorys(PEPROCESS EProcess, PMEMORYS_INFORMATION MemoryInfo, ULONG NumberOfMemory);
NTSTATUS PsReadProcessMem(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS PsWriteProcessMem(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS PsModifyProcessMem(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);

