#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"

#pragma region SYSTEM_MODULE_INFORMATION
typedef struct _SYSTEM_MODULE_ENTRY
{
    ULONG  Unused;
    ULONG  Always0;
    PVOID  ModuleBaseAddress;
    ULONG  ModuleSize;
    ULONG  Unknown;
    ULONG  ModuleEntryIndex;
    USHORT ModuleNameLength;
    USHORT ModuleNameOffset;
    CHAR   ModuleName[256];
} SYSTEM_MODULE_ENTRY, * PSYSTEM_MODULE_ENTRY;

typedef struct _SYSTEM_MODULE_INFORMATION
{
    ULONG               Count;
    SYSTEM_MODULE_ENTRY Module[1];
} SYSTEM_MODULE_INFORMATION, * PSYSTEM_MODULE_INFORMATION;
#pragma endregion


//枚举驱动模块相关结构
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

NTKERNELAPI
NTSTATUS
ObReferenceObjectByName(
    IN PUNICODE_STRING ObjectName,
    IN ULONG Attributes,
    IN PACCESS_STATE PassedAccessState OPTIONAL,
    IN ACCESS_MASK DesiredAccess OPTIONAL,
    IN POBJECT_TYPE ObjectType,
    IN KPROCESSOR_MODE AccessMode,
    IN OUT PVOID ParseContext OPTIONAL,
    OUT PVOID* Object
);


void GetDriverObject(PDRIVER_OBJECT DriverObject);
PVOID GetKernelModuleHandle(PCHAR ModuleName, ULONG* SizeOfImage);
NTSTATUS EnumDriverModule(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);

extern PVOID __DriverSection;