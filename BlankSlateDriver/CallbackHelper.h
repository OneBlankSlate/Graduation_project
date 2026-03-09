#pragma once
#include<fltKernel.h>
#include"DriverEntry.h"
#define CB_PROCESS_TERMINATE 0x0001
#define TD_CALLBACK_REGISTRATION_TAG  '0bCO' // TD_CALLBACK_REGISTRATION structure.
#define TD_CALL_CONTEXT_TAG           '1bCO' // TD_CALL_CONTEXT structure.

#define TD_ASSERT(_exp) \
    ((!(_exp)) ? \
        (__annotation(L"Debug", L"AssertFail", L#_exp), \
         DbgRaiseAssertionFailure(), FALSE) : \
        TRUE)

typedef struct _TD_CALLBACK_PARAMETERS {
    ACCESS_MASK AccessBitsToClear;
    ACCESS_MASK AccessBitsToSet;
}TD_CALLBACK_PARAMETERS, * PTD_CALLBACK_PARAMETERS;

typedef struct _TD_CALLBACK_REGISTRATION {

    //
    // Handle returned by ObRegisterCallbacks.
    //

    PVOID RegistrationHandle;

    //
    // If not NULL, filter only requests to open/duplicate handles to this
    // process (or one of its threads).
    //

    PVOID TargetProcess;
    HANDLE TargetProcessId;


    //
    // Currently each TD_CALLBACK_REGISTRATION has at most one process and one
    // thread callback. That is, we can't register more than one callback for
    // the same object type with a single ObRegisterCallbacks call.
    //

    TD_CALLBACK_PARAMETERS ProcessParams;
    TD_CALLBACK_PARAMETERS ThreadParams;

    ULONG RegistrationId;        // Index in the global TdCallbacks array.

}TD_CALLBACK_REGISTRATION, * PTD_CALLBACK_REGISTRATION;
typedef struct _TD_CALL_CONTEXT
{
    PTD_CALLBACK_REGISTRATION CallbackRegistration;

    OB_OPERATION Operation;
    PVOID Object;
    POBJECT_TYPE ObjectType;
}TD_CALL_CONTEXT, * PTD_CALL_CONTEXT;

void InitializeCallbackSource(PDRIVER_OBJECT DriverObject);
void UninitializeCallbackSource();
NTSTATUS PsProtectProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);
OB_PREOP_CALLBACK_STATUS PreOperationCallback(_In_ PVOID RegistrationContext, _Inout_ POB_PRE_OPERATION_INFORMATION PreInfo);
void TdSetCallContext(_Inout_ POB_PRE_OPERATION_INFORMATION PreInfo, _In_ PTD_CALLBACK_REGISTRATION CallbackRegistration);
NTSTATUS PsUnprotectProcess(PVOID InputBuffer, ULONG InputBufferLength, PVOID OutputBuffer, ULONG OutputBufferLength, ULONG* ReturnValue);