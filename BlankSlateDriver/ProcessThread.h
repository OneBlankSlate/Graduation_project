#pragma once
#include<fltKernel.h>
#define THREAD_CREATE_FLAGS_CREATE_SUSPENDED  0x00000001
#define THREAD_CREATE_FLAGS_SKIP_THREAD_ATTACH 0x00000002
#define THREAD_CREATE_FLAGS_HIDE_FROM_DEBUGGER 0x00000004

typedef struct _THREAD_BASIC_INFORMATION
{
    NTSTATUS ExitStatus;
    PVOID TebBaseAddress;
    CLIENT_ID ClientId;
    KAFFINITY AffinityMask;
    KPRIORITY Priority;
    KPRIORITY BasePriority;
} THREAD_BASIC_INFORMATION, * PTHREAD_BASIC_INFORMATION;
typedef struct _NT_PROC_THREAD_ATTRIBUTE_ENTRY
{
    ULONG Attribute;
    SIZE_T Size;
    ULONG_PTR Value;
    ULONG Unknown;
}NT_PROC_THREAD_ATTRIBUTE_ENTRY, * PNT_PROC_THREAD_ATTRIBUTE_ENTRY;
typedef struct _NT_PROC_THREAD_ATTRIBUTE_LIST
{
    ULONG Length;
    NT_PROC_THREAD_ATTRIBUTE_ENTRY Entry[1];
}NT_PROC_THREAD_ATTRIBUTE_LIST, * PNT_PROC_THREAD_ATTRIBUTE_LIST;
//º¯ÊýÖ¸Õë
typedef NTSTATUS(NTAPI* LPFN_NTCREATETHREADEX)
(
    OUT PHANDLE ThreadHandle,
    IN ACCESS_MASK DesiredAccess,
    IN PVOID ObjectAttributes,
    IN HANDLE ProcessHandle,
    IN PVOID StartAddress,
    IN PVOID Parameter,
    IN ULONG Flags,
    IN SIZE_T StackZeroBits,
    IN SIZE_T SizeOfStackCommit,
    IN SIZE_T SizeOfStackReserve,
    OUT PVOID ByteBuffer);

NTSYSAPI
NTSTATUS
NTAPI
ZwQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ THREADINFOCLASS ThreadInformationClass,
    _Out_ PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Out_opt_ PULONG ReturnLength
);

NTSTATUS PsLookupThreadByEProcess(PEPROCESS EProcess, PETHREAD* EThread);
NTSTATUS CreateThreadEx(OUT PHANDLE ThreadHandle, IN ACCESS_MASK DesiredAccess, IN PVOID ObjectAttributes, IN HANDLE ProcessHandle, IN PVOID StartAddress, IN PVOID ParameterData, IN ULONG Flags, IN SIZE_T StackZeroBits, IN SIZE_T SizeOfStackCommit, IN SIZE_T SizeOfStackReserve, IN PNT_PROC_THREAD_ATTRIBUTE_LIST AttributeList);
NTSTATUS ExecuteInNewThread(IN PVOID BaseAddress, IN PVOID ParameterData, IN ULONG Flags, IN BOOLEAN IsWait, OUT PNTSTATUS ExitStatus);
CHAR ChangePreviousMode(PETHREAD EThread);
VOID RecoverPreviousMode(PETHREAD EThread, CHAR PreviousMode);