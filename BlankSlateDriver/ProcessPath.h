#pragma once
#include<fltKernel.h>
#define MAX_PATH 260
#define PROCESS_QUERY_INFORMATION (0x0400)

typedef struct _PROCESS_PATH_REQUEST_
{
    HANDLE ProcessIdentity;
    WCHAR ProcessPath[MAX_PATH];
}PROCESS_PATH_REQUEST, * PPROCESS_PATH_REQUEST;

BOOLEAN GetProcessFullPathByPeb(PVOID EProcess, WCHAR* ProcessFullPath, ULONG ProcessFullPathLength);
BOOLEAN GetProcessFullPathByEProcess(PVOID EProcess, WCHAR* ProcessFullPath, ULONG ProcessFullPathLength);
NTSTATUS PsGetProcessPath(PPROCESS_PATH_REQUEST ProcessPathRequest);

extern
KPROCESSOR_MODE
NTAPI
PsGetCurrentThreadPreviousMode(VOID);

extern
NTSTATUS NTAPI ZwQueryInformationProcess(
    IN HANDLE ProcessHandle,
    IN PROCESSINFOCLASS ProcessInformationClass,
    OUT PVOID ProcessInformation,
    IN ULONG ProcessInformationLength,
    OUT PULONG ReturnLength OPTIONAL
);


