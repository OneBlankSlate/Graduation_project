#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"
typedef struct _MODULE_INFORMATION_ENTRY_
{
	WCHAR ModulePath[MAX_PATH];
	ULONG_PTR ModuleBase;
	ULONG SizeOfImage;

}MODULE_INFORMATION_ENTRY, * PMODULE_INFORMATION_ENTRY;

typedef struct _MODULES_INFORMATION_
{
	ULONG NumberOfModules;
	MODULE_INFORMATION_ENTRY ModuleInfo[1];
}MODULES_INFORMATION, * PMODULES_INFORMATION;

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

}COMMUNICATE_PROCESS_MODULE, * PCOMMUNICATE_PROCESS_MODULE;

PVOID GetUserModuleHandle(IN PEPROCESS EProcess, IN PUNICODE_STRING ModuleName, ULONG* SizeOfImage);

NTSTATUS PsEnumProcessModules(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS EnumProcessModulesByPeb(PEPROCESS EProcess, PMODULES_INFORMATION ModulesInfo, ULONG NumberOfModule);
void WalkModuleList(PLIST_ENTRY ListEntry, ULONG EnumType, PMODULES_INFORMATION ModulesInfo, ULONG NumberOfModule);
void WalkModuleList2(PLIST_ENTRY ListEntry, ULONG EnumType, ULONG_PTR ModuleBase);
BOOLEAN InModuleList(ULONG_PTR ModuleBase, ULONG SizeOfImage, PMODULES_INFORMATION ModulesInfo, ULONG NumberOfModule);
NTSTATUS PsUnloadProcessModule(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
typedef NTSTATUS (NTAPI* LPFN_NTUNMAPVIEWOFSECTION)(IN HANDLE ProcessHandle, IN PVOID BaseAddress);
typedef NTSTATUS (NTAPI* LPFN_NTCLOSE)(IN HANDLE Handle);
NTSTATUS RemoveProcessModuleInPeb(PEPROCESS EProcess, ULONG_PTR ModuleBase);


extern LPFN_NTCLOSE __NtClose;