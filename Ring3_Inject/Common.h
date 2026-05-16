#pragma once
#include<iostream>
#include<tchar.h>
#include<Windows.h>
#include<TlHelp32.h>
#include<vector> 



typedef LONG KPRIORITY;
typedef struct _UNICODE_STRING {
	USHORT Length;
	USHORT MaximumLength;
	PWSTR  Buffer;
} UNICODE_STRING, * PUNICODE_STRING;

typedef struct _CLIENT_ID
{
	HANDLE UniqueProcess;
	HANDLE UniqueThread;
} CLIENT_ID, * PCLIENT_ID;
typedef struct _SYSTEM_THREAD_INFORMATION
{
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER CreateTime;
	ULONG WaitTime;
	PVOID StartAddress;
	CLIENT_ID ClientId;
	KPRIORITY Priority;
	LONG BasePriority;
	ULONG ContextSwitches;
	ULONG ThreadState;
	ULONG WaitReason;
	ULONG PadPadAlignment;
} SYSTEM_THREAD_INFORMATION, * PSYSTEM_THREAD_INFORMATION;
typedef struct _SYSTEM_PROCESS_INFORMATION
{
	ULONG NextEntryOffset;
	ULONG NumberOfThreads;
	LARGE_INTEGER WorkingSetPrivateSize; //VISTA
	ULONG HardFaultCount; //WIN7
	ULONG NumberOfThreadsHighWatermark; //WIN7
	ULONGLONG CycleTime; //WIN7
	LARGE_INTEGER CreateTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER KernelTime;
	UNICODE_STRING ImageName;
	KPRIORITY BasePriority;
	HANDLE UniqueProcessId;
	HANDLE InheritedFromUniqueProcessId;
	ULONG HandleCount;
	ULONG SessionId;
	ULONG_PTR PageDirectoryBase;

	//
	// This part corresponds to VM_COUNTERS_EX.
	// NOTE: *NOT* THE SAME AS VM_COUNTERS!
	//
	SIZE_T PeakVirtualSize;
	SIZE_T VirtualSize;
	ULONG PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
	SIZE_T PrivatePageCount;

	//
	// This part corresponds to IO_COUNTERS
	//
	LARGE_INTEGER ReadOperationCount;
	LARGE_INTEGER WriteOperationCount;
	LARGE_INTEGER OtherOperationCount;
	LARGE_INTEGER ReadTransferCount;
	LARGE_INTEGER WriteTransferCount;
	LARGE_INTEGER OtherTransferCount;
	SYSTEM_THREAD_INFORMATION TH[1];
} SYSTEM_PROCESS_INFORMATION, * PSYSTEM_PROCESS_INFORMATION;
typedef enum _SYSTEM_INFORMATION_CLASS {
	SystemBasicInformation,
	SystemProcessorInformation,
	SystemPerformanceInformation,
	SystemTimeOfDayInformation,
	SystemPathInformation,
	SystemProcessInformation,
	SystemCallCountInformation,
	SystemDeviceInformation,
	SystemProcessorPerformanceInformation,
	SystemFlagsInformation,
	SystemCallTimeInformation,
	SystemModuleInformation,
	SystemLocksInformation,
	SystemStackTraceInformation,
	SystemPagedPoolInformation,
	SystemNonPagedPoolInformation,
	SystemHandleInformation,
	SystemObjectInformation,
	SystemPageFileInformation,
	SystemVdmInstemulInformation,
	SystemVdmBopInformation,
	SystemFileCacheInformation,
	SystemPoolTagInformation,
	SystemInterruptInformation,
	SystemDpcBehaviorInformation,
	SystemFullMemoryInformation,
	SystemLoadGdiDriverInformation,
	SystemUnloadGdiDriverInformation,
	SystemTimeAdjustmentInformation,
	SystemSummaryMemoryInformation,
	SystemNextEventIdInformation,
	SystemEventIdsInformation,
	SystemCrashDumpInformation,
	SystemExceptionInformation,
	SystemCrashDumpStateInformation,
	SystemKernelDebuggerInformation,
	SystemContextSwitchInformation,
	SystemRegistryQuotaInformation,
	SystemExtendServiceTableInformation,
	SystemPrioritySeperation,
	SystemPlugPlayBusInformation,
	SystemDockInformation,
	SystemProcessorSpeedInformation,
	SystemCurrentTimeZoneInformation,
	SystemLookasideInformation
} SYSTEM_INFORMATION_CLASS;

#define PAGE_SIZE 0x1000
typedef HMODULE(WINAPI* LPFN_LOADLIBRARYW)(LPCWSTR lpLibFileName);
typedef HMODULE(WINAPI* LPFN_LOADLIBRARYA)(LPCSTR lpLibFileName);



#ifdef _WIN64
UINT8	__ShellCode[0x100] = {
	0x48,0x83,0xEC,0x28,	// sub rsp ,28h   //rcx rdx r8 r9  对齐 

	0x48,0x8D,0x0d,			// [+4] lea rcx,    
	0x00,0x00,0x00,0x00,	// [+7] DllFullPathOffset = [+43] - [+4] - 7
	// call 跳偏移，到地址，解*号
	0xff,0x15,				// [+11]
	0x00,0x00,0x00,0x00,	// [+13] LoadLibraryAddressOffset

	0x48,0x83,0xc4,0x28,	// [+17] add rsp,28h

	// jmp 跳偏移，到地址，解*号
	0xff,0x25,				// [+21]
	0x00,0x00,0x00,0x00,	// [+23] Jmp Rip

	// 存放原先的 rip
	0x00,0x00,0x00,0x00,	// [+27]   //
	0x00,0x00,0x00,0x00,	// [+31]

	// 跳板 loadlibrary地址
	0x00,0x00,0x00,0x00,	// [+35] 
	0x00,0x00,0x00,0x00,	// [+39]

	// 存放dll完整路径
	//	0x00,0x00,0x00,0x00,	// [+43]
	//	0x00,0x00,0x00,0x00		// [+47]
	//	......
};

#else
UINT8	__ShellCode[0x100] = {
	0x60,					// [+0] pusha   //其入栈顺序是:EAX,ECX,EDX,EBX,ESP,EBP,ESI,EDI
	0x9c,					// [+1] pushf
	0x68,					// [+2] push
	0x00,0x00,0x00,0x00,	// [+3] ShellCode + 
	0xff,0x15,				// [+7] call	
	0x00,0x00,0x00,0x00,	// [+9] LoadLibrary Addr  Addr
	0x9d,					// [+13] popf
	0x61,					// [+14] popa
	0xff,0x25,				// [+15] jmp
	0x00,0x00,0x00,0x00,	// [+17] jmp  eip

	// eip 地址
	0x00,0x00,0x00,0x00,	// [+21]
	//LoadLibrary地址
	0x00,0x00,0x00,0x00,	// [+25] 
	//DllFullPath 
	0x00,0x00,0x00,0x00		// [+29] 


};
#endif