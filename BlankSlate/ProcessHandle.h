#pragma once
#include<tchar.h>
#include<Windows.h>
#include<iostream>
#include<vector>
#include"IoControlHelper.h"
//进程句柄
typedef struct _HANDLE_INFORMATION_ENTRY_
{
	WCHAR HandleType[0x20];
	WCHAR HandleName[MAX_PATH];
	HANDLE Handle;
	PVOID Object;
	UCHAR Index;   //句柄类型的代号、索引
	ULONG64 Count;   //句柄的引用计数	
}HANDLE_INFORMATION_ENTRY, * PHANDLE_INFORMATION_ENTRY;
typedef struct _HANDLES_INFORMATION_
{
	ULONG NumberOfHandle;
	HANDLE_INFORMATION_ENTRY HandleInfo[1];
}HANDLES_INFORMATION, * PHANDLES_INFORMATION;

typedef struct _COMMUNICATE_PROCESS_HANDLE_
{
	OPERATE_TYPE OperateType;
	HANDLE ProcessIdentity;
}COMMUNICATE_PROCESS_HANDLE, * PCOMMUNICATE_PROCESS_HANDLE;
BOOL EnumProcessHandles(HANDLE ProcessIdentity, vector<HANDLE_INFORMATION_ENTRY>& HandleInfo);