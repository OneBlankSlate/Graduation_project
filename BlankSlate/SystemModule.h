#pragma once
#include<tchar.h>
#include<Windows.h>
#include<iostream>
#include<vector>
#include"IoControlHelper.h"


typedef struct _DRIVER_MODULE_ENTRY_
{
    WCHAR DriverName[MAX_PATH];
    ULONG_PTR ModuleBase;
    ULONG SizeOfImage;
    ULONG_PTR EntryPoint;
    WCHAR DriverPath[MAX_PATH];
}DRIVER_MODULE_ENTRY, * PDRIVER_MODULE_ENTRY;

typedef struct _DRIVER_MODULES_
{
	ULONG NumberOfModules;
	DRIVER_MODULE_ENTRY ModuleEntry[1];
}DRIVER_MODULES, * PDRIVER_MODULES;

void EnumDriverModule(vector<DRIVER_MODULE_ENTRY>& DriverModuleInfo);