#pragma once
#include<fltKernel.h>
#include"IoControlHelper.h"

#define MULTI_OPEN_MAX_ENTRIES 64
#define MULTI_OPEN_IMAGE_NAME_LEN 64

typedef struct _COMMUNICATE_MULTI_OPEN_PREVENT_
{
	OPERATE_TYPE OperateType;
	WCHAR ImageName[MULTI_OPEN_IMAGE_NAME_LEN];
}COMMUNICATE_MULTI_OPEN_PREVENT, * PCOMMUNICATE_MULTI_OPEN_PREVENT;

typedef struct _MULTI_OPEN_ENTRY_ {
	WCHAR ImageName[MULTI_OPEN_IMAGE_NAME_LEN];
	BOOLEAN Active;
} MULTI_OPEN_ENTRY, * PMULTI_OPEN_ENTRY;

typedef struct _MULTI_OPEN_CONTEXT_ {
	MULTI_OPEN_ENTRY Entries[MULTI_OPEN_MAX_ENTRIES];
	FAST_MUTEX Mutex;
	BOOLEAN CallbackRegistered;
} MULTI_OPEN_CONTEXT, * PMULTI_OPEN_CONTEXT;

extern PMULTI_OPEN_CONTEXT g_MultiOpenContext;

VOID InitializeMultiOpenPrevent();
VOID UninitializeMultiOpenPrevent();
NTSTATUS PsPreventMultiOpen(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
NTSTATUS PsCancelMultiOpen(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);

VOID MultiOpenProcessNotifyCallback(
	_Inout_ PEPROCESS Process,
	_In_ HANDLE ProcessId,
	_Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo
);
