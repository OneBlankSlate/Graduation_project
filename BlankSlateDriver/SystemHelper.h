#pragma once
#include<fltKernel.h>

#include <ntimage.h>




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
#if !defined PO_CB_SYSTEM_POWER_POLICY
    SystemPowerInformation,
#else
    _SystemPowerInformation,
#endif
    SystemProcessorSpeedInformation,
    SystemCurrentTimeZoneInformation,
    SystemLookasideInformation
} SYSTEM_INFORMATION_CLASS;

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



NTSYSAPI NTSTATUS NTAPI ZwQuerySystemInformation(
    IN SYSTEM_INFORMATION_CLASS SystemInfoClass,
    OUT PVOID SystemInfoBuffer,
    IN ULONG SystemInfoBufferSize,
    OUT PULONG BytesReturned OPTIONAL
);

#pragma region SYSTEM_SERVICE_DESCRIPTOR_TABLE
typedef struct _SYSTEM_SERVICE_DESCRIPTOR_TABLE
{
	PLONG32 ServiceTableBase;
	PLONG32 ServiceCounterTableBase;
	ULONG_PTR NumberOfServices;
	PUCHAR ParamterTableBase;
}SYSTEM_SERVICE_DESCRIPTOR_TABLE, *PSYSTEM_SERVICE_DESCRIPTOR_TABLE;

typedef struct _SYSTEM_SERVICE_DESCRIPTOR_TABLE_SHADOW
{
    PLONG32 ServiceTableBase1;
    PLONG32 ServiceCounterTableBase1;
    ULONG_PTR NumberOfServices1;
    PUCHAR ParamterTableBase1;

    PLONG32 ServiceTableBase2;
    PLONG32 ServiceCounterTableBase2;
    ULONG_PTR NumberOfServices2;
    PUCHAR ParamterTableBase2;
}SYSTEM_SERVICE_DESCRIPTOR_TABLE_SHADOW, * PSYSTEM_SERVICE_DESCRIPTOR_TABLE_SHADOW;
#pragma endregion


BOOLEAN GetNtXXXServiceIndex(CHAR* FunctionName, ULONG32* ServiceIndex);
PSYSTEM_SERVICE_DESCRIPTOR_TABLE GetKeServiceDescriptorTable();
PVOID GetNtoskrnlInfo(OUT PUNICODE_STRING NtoskrnlPath, OUT PULONG ImageSize);
NTSTATUS GetNtXXXServiceAddress(ULONG_PTR ServiceIndex, PVOID* ServiceAddress);
VOID FreeNtoskrnlInfo();
PEPROCESS LookupWin32Process();
PIMAGE_NT_HEADERS RtlImageNtHeader(PVOID Base);
VOID InitializeSystemSource();
VOID UninitializeSystemSource();
//游戏驱动中的GetFunc
PVOID64 GetSSDTAddre();
PVOID GetSSDTServiceAddress(IN wchar_t* FuncName);  //这里要传Zw函数，然后后续得Nt



extern PSYSTEM_SERVICE_DESCRIPTOR_TABLE __SystemServiceDescriptorTable;  //自己的全局
extern PSYSTEM_SERVICE_DESCRIPTOR_TABLE KeServiceDescriptorTable;   //系统的全局
extern PVOID PsGetProcessWin32Process(PEPROCESS Process);
extern PVOID __Ntoskrnl;
extern PEPROCESS __SystemEProcess;
