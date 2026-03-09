#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"

#define PROCESS_TERMINATE                       0x0001
#define PROCESS_CREATE_THREAD                   0x0002
#define PROCESS_SET_SESSIONID                   0x0004
#define PROCESS_VM_OPERATION                    0x0008
#define PROCESS_VM_READ                         0x0010
#define PROCESS_VM_WRITE                        0x0020
#define PROCESS_CREATE_PROCESS                  0x0080
#define PROCESS_SET_QUOTA                       0x0100
#define PROCESS_SET_INFORMATION                 0x0200
#define PROCESS_QUERY_INFORMATION               0x0400
#define PROCESS_SUSPEND_RESUME                  0x0800
#define PROCESS_QUERY_LIMITED_INFORMATION       0x1000


#define MAX_PATH 260
#define RTL_MAX_DRIVE_LETTERS 32
#pragma region MODULE
typedef struct _RTL_PROCESS_MODULE_INFORMATION
{
	HANDLE Section;
	PVOID MappedBase;
	PVOID ImageBase;
	ULONG ImageSize;
	ULONG Flags;
	USHORT LoadOrderIndex;
	USHORT InitOrderIndex;
	USHORT LoadCount;
	USHORT OffsetToFileName;
	UCHAR FullPathName[MAXIMUM_FILENAME_LENGTH];
}RTL_PROCESS_MODULE_INFORMATION, *PRTL_PROCESS_MODULE_INFORMATION;


typedef struct _RTL_PROCESS_MODULES
{
	ULONG NumberOfModules;
	RTL_PROCESS_MODULE_INFORMATION Modules[1];
}RTL_PROCESS_MODULES, * PRTL_PROCESS_MODULES;
#pragma endregion


#pragma region _PEB_INFORMATION_
typedef struct _PEB_LDR_DATA32 {
    ULONG          Length;
    BOOLEAN        Initialized;
    ULONG          SsHandle;
    LIST_ENTRY32   InLoadOrderModuleList;
    LIST_ENTRY32   InMemoryOrderModuleList;
    LIST_ENTRY32   InInitializationOrderModuleList;
} PEB_LDR_DATA32, * PPEB_LDR_DATA32;
typedef struct _PEB_LDR_DATA {
    ULONG          Length;
    BOOLEAN        Initialized;
    HANDLE         SsHandle;
    LIST_ENTRY     InLoadOrderModuleList;
    LIST_ENTRY     InMemoryOrderModuleList;
    LIST_ENTRY     InInitializationOrderModuleList;
} PEB_LDR_DATA, * PPEB_LDR_DATA;

typedef struct _LDR_DATA_TABLE_ENTRY32 {
    LIST_ENTRY32     InLoadOrderLinks;
    LIST_ENTRY32     InMemoryOrderLinks;
    LIST_ENTRY32     InInitializationOrderLinks;
    ULONG            ModuleBaseAddress;
    ULONG            EntryPoint;
    ULONG            ModuleSize;
    UNICODE_STRING32 FullModuleName;
    UNICODE_STRING32 ModuleName;
    ULONG            Flags;
    USHORT           LoadCount;
    USHORT           TlsIndex;
    LIST_ENTRY32     HashLinks;
    ULONG            TimeStamp;
} LDR_DATA_TABLE_ENTRY32, * PLDR_DATA_TABLE_ENTRY32;
typedef struct _LDR_DATA_TABLE_ENTRY {
    LIST_ENTRY     InLoadOrderLinks;
    LIST_ENTRY     InMemoryOrderLinks;
    LIST_ENTRY     InInitializationOrderLinks;
    PVOID          ModuleBaseAddress;
    PVOID          EntryPoint;
    ULONG          ModuleSize;
    UNICODE_STRING FullModuleName;
    UNICODE_STRING ModuleName;
    ULONG          Flags;
    USHORT         LoadCount;
    USHORT         TlsIndex;
    LIST_ENTRY     HashLinks;
    ULONG          TimeStamp;
} LDR_DATA_TABLE_ENTRY, * PLDR_DATA_TABLE_ENTRY;
typedef struct _CURDIR
{
    UNICODE_STRING DosPath;
    HANDLE Handle;
} CURDIR, * PCURDIR;
typedef struct _RTL_DRIVE_LETTER_CURDIR
{
    USHORT Flags;
    USHORT Length;
    ULONG TimeStamp;
    UNICODE_STRING DosPath;
} RTL_DRIVE_LETTER_CURDIR, * PRTL_DRIVE_LETTER_CURDIR;
typedef struct _RTL_USER_PROCESS_PARAMETERS
{
    ULONG MaximumLength;
    ULONG Length;
    ULONG Flags;
    ULONG DebugFlags;
    HANDLE ConsoleHandle;
    ULONG ConsoleFlags;
    HANDLE StandardInput;
    HANDLE StandardOutput;
    HANDLE StandardError;
    CURDIR CurrentDirectory;
    UNICODE_STRING DllPath;
    UNICODE_STRING ImagePathName;
    UNICODE_STRING CommandLine;
    PWSTR Environment;
    ULONG StartingX;
    ULONG StartingY;
    ULONG CountX;
    ULONG CountY;
    ULONG CountCharsX;
    ULONG CountCharsY;
    ULONG FillAttribute;
    ULONG WindowFlags;
    ULONG ShowWindowFlags;
    UNICODE_STRING WindowTitle;
    UNICODE_STRING DesktopInfo;
    UNICODE_STRING ShellInfo;
    UNICODE_STRING RuntimeData;
    RTL_DRIVE_LETTER_CURDIR CurrentDirectories[RTL_MAX_DRIVE_LETTERS];
#if (NTDDI_VERSION >= NTDDI_LONGHORN)
    SIZE_T EnvironmentSize;
#endif
#if (NTDDI_VERSION >= NTDDI_WIN7)
    SIZE_T EnvironmentVersion;
#endif
} RTL_USER_PROCESS_PARAMETERS, * PRTL_USER_PROCESS_PARAMETERS;
typedef struct _PEB32
{
    BOOLEAN                      InheritedAddressSpace;
    BOOLEAN                      ReadImageFileExecOptions;
    BOOLEAN                      BeingDebugged;
    BOOLEAN                      SpareBool;
    ULONG                        Mutant;
    ULONG                        ImageBaseAddress;
    ULONG                        Ldr;
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
} PEB32, * PPEB32;
typedef struct _PEB
{                                                                 
    BOOLEAN                      InheritedAddressSpace;           
    BOOLEAN                      ReadImageFileExecOptions;        
    BOOLEAN                      BeingDebugged;                   
    BOOLEAN                      SpareBool;                       
    HANDLE                       Mutant;                          
    PVOID                        ImageBaseAddress;                  
    PPEB_LDR_DATA                Ldr;  
    PRTL_USER_PROCESS_PARAMETERS ProcessParameters;
} PEB, * PPEB;
#pragma endregion
//进程基本信息
typedef struct _PROCESS_INFORMATION_ENTRY_
{
    char ImageName[15];
    ULONG_PTR ProcessIdentity;
    ULONG_PTR ParentPid;
    WCHAR ProcessPath[MAX_PATH];
    PVOID EProcess;
}PROCESS_INFORMATION_ENTRY, * PPROCESS_INFORMATION_ENTRY;

typedef struct _PROCESS_INFORMATIONS_
{
    ULONG NumberOfProcess;
    PROCESS_INFORMATION_ENTRY ProcessInfo[1];
}PROCESS_INFORMATIONS, * PPROCESS_INFORMATIONS;
//进程保护
typedef struct _COMMUNICATE_PROTECT_PROCESS_
{
    OPERATE_TYPE OperateType;
    HANDLE ProcessIdentitys;
}COMMUNICATE_PROTECT_PROCESS, * PCOMMUNICATE_PROTECT_PROCESS;

//进程隐藏
typedef struct _COMMUNICATE_HIDE_PROCESS_
{
    OPERATE_TYPE OperateType;
    HANDLE ProcessIdentity;
}COMMUNICATE_HIDE_PROCESS, * PCOMMUNICATE_HIDE_PROCESS;

//结束进程
typedef struct _COMMUNICATE_TERMINATE_PROCESS_
{
    OPERATE_TYPE OperateType;
    HANDLE ProcessIdentity;
}COMMUNICATE_TERMINATE_PROCESS, * PCOMMUNICATE_TERMINATE_PROCESS;




typedef NTSTATUS(__fastcall* LPFN_NTTERMINATEPROCESS)(
    _In_opt_ HANDLE ProcessHandle,
    _In_ NTSTATUS ExitStatus
    );
BOOLEAN PsIsProcessTermination(PEPROCESS EProcess);
NTKERNELAPI HANDLE NTAPI PsGetProcessInheritedFromUniqueProcessId(PEPROCESS Process);
BOOLEAN CheckNtdll32(ULONG VirtualAddress, PULONG DllBase);
BOOLEAN CheckNtdll64(ULONG VirtualAddress, PULONG_PTR DllBase);
NTKERNELAPI PPEB NTAPI PsGetProcessPeb(PEPROCESS Process);
NTKERNELAPI PVOID NTAPI PsGetCurrentProcessWow64Process();
BOOLEAN PsIsRealProcess(PEPROCESS EProcess);
NTSTATUS PsEnumProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS PsEnumProcessInternal(PPROCESS_INFORMATIONS ProcessInfos, ULONG OutputBufferLength);
VOID EnumProcessByService(PPROCESS_INFORMATIONS ProcessInfos, ULONG NumberOfProcess);
VOID SetProcessInfoToList(PPROCESS_INFORMATIONS ProcessInfos, ULONG NumberOfProcess, PEPROCESS EProcess);
NTSTATUS SafeCopyProcessModule(PEPROCESS EProcess, ULONG_PTR ModuleBase, ULONG SizeOfImage, PVOID OutputBuffer);
BOOLEAN PsIsProcessTermination(PEPROCESS EProcess);

NTSTATUS PsTerminateProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS PsHideProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);