#pragma once
#include<tchar.h>
#include<Windows.h>
#include<iostream>
#include<vector>
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
}COMMUNICATE_PROCESS_MEMORY,*PCOMMUNICATE_PROCESS_MEMORY;


typedef struct _MEMORY_INFORMATION_ENTRY_
{
	PVOID BaseAddress;
	SIZE_T RegionSize;
	ULONG Protect;
	ULONG State;
	ULONG Type;
}MEMORY_INFORMATION_ENTRY,*PMEMORY_INFORMATION_ENTRY;
typedef struct _MEMORYS_INFORMATION_
{
	ULONG NumberOfMemory;
	MEMORY_INFORMATION_ENTRY MemoryInfo[1];
}MEMORYS_INFORMATION,*PMEMORYS_INFORMATION;

BOOL EnumProcessMemorys(HANDLE ProcessIdentity, vector<MEMORY_INFORMATION_ENTRY>& MemoryInfo);
//将得到的数值转为真正的属性
const WCHAR* GetProtect(ULONG Protect);
const WCHAR* GetState(ULONG State);
const WCHAR* GetType(ULONG Type);

