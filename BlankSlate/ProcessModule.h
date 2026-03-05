#pragma once
#include<tchar.h>
#include<Windows.h>
#include"IoControlHelper.h"
#include<iostream>
#include<vector>
#include<QList>
#include<QStandardItem>
using namespace std;

typedef struct _MODULE_INFORMATION_ENTRY_
{
	WCHAR ModulePath[MAX_PATH];
	ULONG_PTR ModuleBase;
	ULONG SizeOfImage;
	
}MODULE_INFORMATION_ENTRY,*PMODULE_INFORMATION_ENTRY;

typedef struct _MODULES_INFORMATION_
{
	ULONG NumberOfModules;
	MODULE_INFORMATION_ENTRY ModuleInfo[1];
}MODULES_INFORMATION,*PMODULES_INFORMATION;

typedef struct _COMMUNICATE_PROCESS_MODULE_
{
	OPERATE_TYPE OperateType;
	ULONG_PTR ProcessIdentity;
	union {
		struct {
			ULONG_PTR ModuleBase;
			ULONG SizeOfImage;
		}Dump;
		struct {
			ULONG_PTR ModuleBase;
			ULONG_PTR LdrpHashTable;
		}Unload;
	}u1;

}COMMUNICATE_PROCESS_MODULE,*PCOMMUNICATE_PROCESS_MODULE;

BOOL EnumProcessModules(ULONG_PTR ProcessIdentity, vector<MODULE_INFORMATION_ENTRY>& ModuleInfo);




